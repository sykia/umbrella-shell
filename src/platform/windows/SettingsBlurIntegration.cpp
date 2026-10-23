#include "SettingsPageObserver.h"
#include "core/settings/SettingsPagePolicy.h"
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <Windows.h>
#include <dwmapi.h>

namespace umbrella::platform {
bool isSettingsWindow(HWND window);
bool settingsClient(HWND window, settings::Rect& result);
struct SettingsBlurIntegration::Impl final : QAbstractNativeEventFilter {
    QObject context;
    QTimer debounce, deadline;
    QProcess observer;
    QByteArray output;
    std::unique_ptr<QQuickView> view;
    HWND target{};
    HWND overlayWindow{};
    settings::Rect requestedClient;
    HWINEVENTHOOK foregroundHook{}, windowHook{};
    HANDLE job{};
    quint64 epoch{}, requestedEpoch{};
    QElapsedTimer scanWindow;
    unsigned scans{};
    quint64 frames{};
    QString reason = "inactive";
    QVariantMap lastReply;
    bool active{}, inFlight{}, rescan{}, failed{}, qmlFailed{}, restartPending{};
    inline static Impl* current{};
    Impl() {
        debounce.setSingleShot(true); debounce.setInterval(100);
        deadline.setSingleShot(true); deadline.setInterval(2500);
        QObject::connect(&debounce, &QTimer::timeout, &context, [this] { scan(); });
        QObject::connect(&deadline, &QTimer::timeout, &context, [this] { fail("observer-deadline"); });
        QObject::connect(&observer, &QProcess::started, &context, [this] {
            // No orphan UIA helper after an abrupt Umbrella exit.
            job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(observer.processId()));
            const bool owned = job && process && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) && AssignProcessToJobObject(job, process);
            if (process) CloseHandle(process);
            if (!owned) { fail("job-assignment"); return; }
            invalidate();
        });
        QObject::connect(&observer, &QProcess::readyReadStandardOutput, &context, [this] {
            output += observer.readAllStandardOutput();
            if (output.size() > 8192) { fail(); return; }
            while (output.contains('\n')) {
                const auto end = output.indexOf('\n');
                const auto line = output.left(end); output.remove(0, end + 1);
                const auto reply = QJsonDocument::fromJson(line).object();
                if (reply["invalidate"].toBool()) { invalidate(); continue; }
                lastReply = reply.toVariantMap();
                deadline.stop(); inFlight = false;
                if (reply["epoch"].toString().toULongLong() == requestedEpoch && requestedEpoch == epoch && active) present(reply);
                if (rescan) { rescan = false; debounce.start(); }
            }
        });
        QObject::connect(&observer, &QProcess::errorOccurred, &context, [this](QProcess::ProcessError) { if (active && !restartPending) fail(); });
        QObject::connect(&observer, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &context, [this](int, QProcess::ExitStatus) {
            hide(); deadline.stop(); inFlight = false;
            if (job) { CloseHandle(job); job = nullptr; }
            if (active && restartPending) { restartPending = false; start(); return; }
            if (active) failed = true; // No crash/restart loop; disable/re-enable to retry.
        });
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
    ~Impl() {
        stop(); QCoreApplication::instance()->removeNativeEventFilter(this);
        observer.disconnect(&context);
        if (job) { CloseHandle(job); job = nullptr; }
        if (observer.state() != QProcess::NotRunning) { observer.kill(); observer.waitForFinished(1000); }
    }
    void hide() { if (view && view->isVisible()) { view->hide(); view->releaseResources(); } }
    void fail(const QString& error = "observer-failure") { if (!failed) reason = error; failed = true; hide(); debounce.stop(); deadline.stop(); if (observer.state() != QProcess::NotRunning) observer.kill(); }
    void stop() {
        active = false; restartPending = false; ++epoch; hide(); debounce.stop(); deadline.stop();
        reason = "inactive";
        if (foregroundHook) { UnhookWinEvent(foregroundHook); foregroundHook = nullptr; }
        if (windowHook) { UnhookWinEvent(windowHook); windowHook = nullptr; }
        if (current == this) current = nullptr;
        target = nullptr; overlayWindow = nullptr; view.reset();
        if (job) { CloseHandle(job); job = nullptr; }
        observer.closeWriteChannel();
    }
    static void CALLBACK event(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG, DWORD, DWORD) {
        auto* self = current;
        if (!self || !self->active) return;
        if (event == EVENT_SYSTEM_FOREGROUND) { self->invalidate(); return; }
        if (window && self->target && (window == self->target || GetAncestor(window, GA_ROOT) == self->target)) {
            // Pointer/focus events inside the overlay must not make it flicker.
            if (object == OBJID_WINDOW || event == EVENT_OBJECT_NAMECHANGE || event == EVENT_OBJECT_STATECHANGE) self->invalidate();
        }
    }
    void invalidate() {
        ++epoch; hide();
        if (!active || failed) return;
        reason = "awaiting-scan";
        if (inFlight) rescan = true;
        else if (!debounce.isActive()) debounce.start();
    }
    void start() {
        if (observer.state() != QProcess::NotRunning) { restartPending = true; return; }
        failed = false; qmlFailed = false; output.clear(); inFlight = false; rescan = false;
        reason = "starting-observer";
        scanWindow.start(); scans = 0;
        current = this;
        // Include our own main window too: a transition to Umbrella must hide the overlay.
        foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, event, 0, 0, WINEVENT_OUTOFCONTEXT);
        windowHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_NAMECHANGE, nullptr, event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (!foregroundHook || !windowHook) { fail(); return; }
        observer.setProgram(QCoreApplication::applicationFilePath());
        observer.setArguments({"--settings-blur-observer"});
        observer.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
        observer.start();
        deadline.start();
    }
    void scan() {
        if (!active || failed || observer.state() != QProcess::Running) return;
        if (inFlight) { rescan = true; return; }
        target = GetAncestor(GetForegroundWindow(), GA_ROOT);
        // Send zero as well, to release the previous UIA root/subscriptions on navigation away.
        if (!isSettingsWindow(target) || !settingsClient(target, requestedClient)) {
            target = nullptr; overlayWindow = nullptr; view.reset();
        }
        reason = target ? "awaiting-uia" : "not-foreground-settings";
        if (scanWindow.elapsed() >= 2000) { scanWindow.restart(); scans = 0; }
        if (target && ++scans > 8) { fail(); return; } // Bound event storms; leave Settings usable.
        requestedEpoch = epoch;
        const auto command = QByteArray::number(epoch) + ' ' + QByteArray::number(reinterpret_cast<quintptr>(target), 16) + '\n';
        if (observer.bytesToWrite() > 256 || observer.write(command) != command.size()) { fail(); return; }
        inFlight = true; deadline.start();
    }
    bool theme(bool& dark, bool& animations, QColor& accent) {
        HIGHCONTRASTW contrast{sizeof(contrast)}; BOOL animate{};
        if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) || (contrast.dwFlags & HCF_HIGHCONTRASTON)) return false;
        if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0)) animate = FALSE;
        QSettings preferences("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
        if (!preferences.value("EnableTransparency", 1).toBool()) return false;
        dark = !preferences.value("AppsUseLightTheme", 1).toBool(); animations = animate != FALSE;
        DWORD color{}; BOOL opaque{};
        accent = SUCCEEDED(DwmGetColorizationColor(&color, &opaque)) ? QColor::fromRgb(color & 0xffffff) : QColor(dark ? "#8cbfff" : "#005fb8");
        return true;
    }
    void present(const QJsonObject& reply) {
        if (!target) { reason = "not-foreground-settings"; return; }
        if (GetAncestor(GetForegroundWindow(), GA_ROOT) != target || !isSettingsWindow(target)) { reason = "target-changed"; return; }
        if (reply["window"].toString() != QString::number(reinterpret_cast<quintptr>(target), 16)) { reason = "reply-window-mismatch"; return; }
        settings::Rect client{};
        if (!settingsClient(target, client) || client.left != requestedClient.left || client.top != requestedClient.top || client.right != requestedClient.right || client.bottom != requestedClient.bottom) { reason = "geometry-changed"; return; }
        const auto rect = reply["rect"].toArray();
        if (rect.size() != 4) { reason = "unrecognized-uia"; return; }
        settings::Rect area{rect[0].toInt(), rect[1].toInt(), rect[2].toInt(), rect[3].toInt()};
        if (!client.contains(area)) { reason = "bounds-outside-client"; return; }
        const UINT dpi = GetDpiForWindow(target);
        if (area.right - area.left < MulDiv(280, static_cast<int>(dpi), 96)
            || area.bottom - area.top < MulDiv(210, static_cast<int>(dpi), 96)) { reason = "viewport-too-small"; return; }
        bool dark{}, animations{}; QColor accent;
        if (!theme(dark, animations, accent)) { reason = "accessibility-or-transparency"; return; }
        if (!view) {
            view = std::make_unique<QQuickView>();
            frames = 0;
            view->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
            view->setTitle(QStringLiteral("Umbrella — системные настройки панели задач"));
            view->setPersistentGraphics(false); view->setPersistentSceneGraph(false);
            view->setResizeMode(QQuickView::SizeRootObjectToView);
            auto format = view->format(); format.setAlphaBufferSize(8); view->setFormat(format); view->setColor(Qt::transparent);
            QObject::connect(view->engine(), &QQmlEngine::warnings, &context, [this] { qmlFailed = true; hide(); });
            QObject::connect(view.get(), &QQuickWindow::sceneGraphError, &context, [this] { qmlFailed = true; hide(); });
            QObject::connect(view.get(), &QQuickWindow::frameSwapped, &context, [this] { ++frames; }, Qt::QueuedConnection);
            view->setSource(QUrl("qrc:/qt/qml/Umbrella/SystemTaskbarOverlay.qml"));
        }
        if (view->status() != QQuickView::Ready || !view->rootObject() || qmlFailed) { reason = "qml-failure"; return; }
        view->rootObject()->setProperty("darkTheme", dark);
        view->rootObject()->setProperty("animationsEnabled", animations);
        view->rootObject()->setProperty("accentColor", accent);
        const auto hwnd = reinterpret_cast<HWND>(view->winId());
        overlayWindow = hwnd;
        // Settings rejected cross-process ownership on the observed build.
        // Associate by verified HWND + lifecycle events instead. Topmost applies
        // only while that Settings window is foreground; never activate our window.
        const int backdrop = DWMSBT_TRANSIENTWINDOW, corner = DWMWCP_DONOTROUND;
        const BOOL useDark = dark;
        const MARGINS glass{-1, -1, -1, -1};
        // Documented Windows 11 22H2+ Desktop Acrylic, on our own window only.
        if (FAILED(DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark)))
            || FAILED(DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop)))
            || FAILED(DwmExtendFrameIntoClientArea(hwnd, &glass))) { reason = "acrylic-unavailable"; return; }
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
        // UIA uses physical coordinates; SetWindowPos avoids Qt logical/physical origin ambiguity.
        if (!SetWindowPos(hwnd, HWND_TOPMOST, area.left, area.top, area.right - area.left, area.bottom - area.top, SWP_NOACTIVATE)) { reason = QString("position-error:%1").arg(GetLastError()); return; }
        if (GetAncestor(GetForegroundWindow(), GA_ROOT) != target) { reason = "foreground-changed-before-show"; return; }
        view->show();
        SetWindowPos(hwnd, HWND_TOPMOST, area.left, area.top, area.right - area.left, area.bottom - area.top, SWP_NOACTIVATE);
        const POINT center{area.left + (area.right - area.left) / 2, area.top + (area.bottom - area.top) / 2};
        if (GetAncestor(WindowFromPoint(center), GA_ROOT) != hwnd) {
            hide(); reason = "overlay-occluded"; return;
        }
        reason = "shown";
    }
    bool nativeEventFilter(const QByteArray&, void* message, qintptr* result) override {
        auto* event = static_cast<MSG*>(message);
        if (overlayWindow && event->hwnd == overlayWindow && event->message == WM_MOUSEACTIVATE) {
            *result = MA_NOACTIVATEANDEAT; return true;
        }
        if (active && (event->message == WM_SETTINGCHANGE || event->message == WM_THEMECHANGED || event->message == WM_DISPLAYCHANGE || event->message == WM_DWMCOLORIZATIONCOLORCHANGED)) invalidate();
        return false;
    }
};
SettingsBlurIntegration::SettingsBlurIntegration() : impl_(std::make_unique<Impl>()) {}
SettingsBlurIntegration::~SettingsBlurIntegration() = default;
bool SettingsBlurIntegration::visible() const { return impl_->view && impl_->view->isVisible(); }
QVariantMap SettingsBlurIntegration::diagnostics() const {
    const auto& d = *impl_;
    RECT overlay{};
    if (d.overlayWindow) GetWindowRect(d.overlayWindow, &overlay);
    return {{"active", d.active}, {"failed", d.failed}, {"inFlight", d.inFlight}, {"reason", d.reason},
        {"observerState", static_cast<int>(d.observer.state())}, {"job", d.job != nullptr},
        {"foregroundHook", d.foregroundHook != nullptr}, {"windowHook", d.windowHook != nullptr},
        {"target", QString::number(reinterpret_cast<quintptr>(d.target), 16)},
        {"foreground", QString::number(reinterpret_cast<quintptr>(GetAncestor(GetForegroundWindow(), GA_ROOT)), 16)},
        {"epoch", QString::number(d.epoch)}, {"requestedEpoch", QString::number(d.requestedEpoch)},
        {"client", QVariantList{d.requestedClient.left, d.requestedClient.top, d.requestedClient.right, d.requestedClient.bottom}},
        {"visible", visible()}, {"exposed", d.view && d.view->isExposed()}, {"frames", QString::number(d.frames)},
        {"overlayRect", QVariantList{static_cast<int>(overlay.left), static_cast<int>(overlay.top), static_cast<int>(overlay.right), static_cast<int>(overlay.bottom)}},
        {"lastReply", d.lastReply}, {"platform", QGuiApplication::platformName()},
        {"overlayWindow", QString::number(reinterpret_cast<quintptr>(d.overlayWindow), 16)},
        {"nativeOverlay", d.overlayWindow && IsWindow(d.overlayWindow)},
        {"owner", QString::number(reinterpret_cast<quintptr>(d.overlayWindow ? GetWindow(d.overlayWindow, GW_OWNER) : nullptr), 16)},
        {"overlayStyle", QString::number(d.overlayWindow ? GetWindowLongPtrW(d.overlayWindow, GWL_STYLE) : 0, 16)}};
}
void SettingsBlurIntegration::update(bool enabled, bool managing) {
    const bool active = enabled && managing;
    if (active == impl_->active) return;
    if (!active) impl_->stop();
    else { impl_->active = true; impl_->start(); }
}
}
