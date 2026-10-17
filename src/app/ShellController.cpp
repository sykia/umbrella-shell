#include "ShellController.h"
#include <QGuiApplication>
#include <QQmlContext>
#include <QScreen>
#include <QTimer>
#include <QUuid>
#include <QDir>
using namespace umbrella::platform;
namespace {
QString executable() { return QDir::toNativeSeparators(QCoreApplication::applicationFilePath()); }
void load(QQuickView& view, ShellController* controller, const QString& name) {
    view.rootContext()->setContextProperty("shell", controller);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qt/qml/Umbrella/" + name + ".qml"));
}
}
ShellController::ShellController(QObject* parent) : QObject(parent),
    settings_(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Shell") {
    enabled_ = settings_.value("shell/replacement", true).toBool();
    connect(&guard_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        if (active_) { restore(); enabled_ = false; settings_.setValue("shell/replacement", false); settings_.sync(); bars_.clear(); qApp->quit(); }
    });
}
ShellController::~ShellController() {
    restore(); bars_.clear();
    UnregisterHotKey(nullptr, 1); qApp->removeNativeEventFilter(this);
    if (mutex_) { ReleaseMutex(mutex_); CloseHandle(mutex_); }
}
bool ShellController::start() {
    mutex_ = CreateMutexW(nullptr, TRUE, L"Local\\UmbrellaShell.DesktopBar");
    if (!mutex_ || GetLastError() == ERROR_ALREADY_EXISTS) return false;
    if (settings_.value("recovery/pending", false).toBool()) {
        if (!setAutoHide(settings_.value("recovery/autoHide", false).toBool())) {
            enabled_ = false; fail(tr("Не удалось восстановить состояние предыдущего запуска."));
        } else { settings_.remove("recovery"); settings_.sync(); }
    }
    qApp->installNativeEventFilter(this);
    if (!RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x55)) {
        fail(tr("Ctrl+Alt+U занято. Используйте кнопку восстановления или --restore-taskbar."));
    }
    connect(qApp, &QGuiApplication::screenAdded, this, [this](QScreen*) { rebuild(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { rebuild(); });
    if (enabled_) rebuild(); else openSettings();
    return true;
}
bool ShellController::startup() const { return startupEnabled(executable().toStdWString()); }
void ShellController::setStartup(bool enabled) {
    if (!umbrella::platform::setStartup(enabled, executable().toStdWString())) fail(tr("Не удалось изменить автозапуск."));
    emit changed();
}
void ShellController::setReplacement(bool enabled) {
    if (enabled_ == enabled) return;
    settings_.setValue("shell/replacement", enabled); settings_.sync();
    if (settings_.status() != QSettings::NoError) { fail(tr("Не удалось сохранить настройки.")); return; }
    enabled_ = enabled;
    if (enabled) rebuild(); else { restore(); bars_.clear(); status_ = tr("Панель Windows включена."); }
    emit changed();
}
void ShellController::fail(const QString& error) { status_ = error; emit changed(); openSettings(); }
void ShellController::openSettings() {
    if (!settingsView_) {
        settingsView_ = std::make_unique<QQuickView>();
        settingsView_->setTitle(tr("Umbrella Shell — Настройки"));
        load(*settingsView_, this, "Settings"); settingsView_->resize(680, 440);
    }
    settingsView_->show(); settingsView_->raise(); settingsView_->requestActivate();
}
void ShellController::quit() { restore(); qApp->quit(); }
void ShellController::restore() {
    const bool wasActive = active_; active_ = false;
    if (stop_) SetEvent(stop_);
    if (guard_.state() != QProcess::NotRunning) guard_.waitForFinished(5000);
    if (wasActive) {
        if (setAutoHide((original_ & ABS_AUTOHIDE) != 0)) { settings_.remove("recovery"); settings_.sync(); }
        else status_ = tr("Windows не подтвердила восстановление. Выполните --restore-taskbar.");
    }
    if (ready_) { CloseHandle(ready_); ready_ = nullptr; }
    if (stop_) { CloseHandle(stop_); stop_ = nullptr; }
}
void ShellController::rebuild() {
    if (rebuilding_ || !enabled_) return;
    rebuilding_ = true;
    restore(); bars_.clear();
    for (auto* screen : QGuiApplication::screens()) {
        // Unique connections are not available for lambdas: reconnect this receiver only.
        disconnect(screen, nullptr, this, nullptr);
        connect(screen, &QScreen::geometryChanged, this, [this] { rebuild(); });
        connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this] { rebuild(); });
        auto bar = std::make_unique<Bar>();
        bar->view = std::make_unique<QQuickView>();
        auto* view = bar->view.get();
        view->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        view->setScreen(screen);
        const QRect rect = screen->geometry();
        view->setGeometry(rect.x(), rect.bottom() - 55, rect.width(), 56);
        load(*view, this, "Bar");
        if (view->status() == QQuickView::Error) { rebuilding_ = false; bars_.clear(); fail(tr("Ошибка загрузки QML панели.")); return; }
        Bar* state = bar.get();
        connect(view, &QQuickWindow::frameSwapped, view, [this, state] {
            if (!state->painted) { state->painted = true; activate(); }
        }, Qt::QueuedConnection);
        connect(view, &QQuickWindow::sceneGraphError, this, [this](QQuickWindow::SceneGraphError, const QString& message) {
            restore(); fail(tr("Ошибка графики: ") + message);
        });
        view->show();
        bars_.push_back(std::move(bar));
    }
    rebuilding_ = false;
}
void ShellController::activate() {
    if (rebuilding_ || active_ || !enabled_ || bars_.empty()) return;
    for (const auto& bar : bars_) if (!bar->painted) return;
    original_ = taskbarState();
    const QString token = "Local\\UmbrellaShell." + QUuid::createUuid().toString(QUuid::Id128);
    const QString readyName = token + ".ready", stopName = "Local\\UmbrellaShell.Recovery";
    ready_ = CreateEventW(nullptr, TRUE, FALSE, readyName.toStdWString().c_str());
    stop_ = CreateEventW(nullptr, TRUE, FALSE, stopName.toStdWString().c_str());
    if (!ready_ || !stop_) { restore(); fail(tr("Не удалось создать канал восстановления.")); return; }
    guard_.start(executable(), {"--guard", QString::number(GetCurrentProcessId()), QString::number(original_), readyName, stopName});
    if (!guard_.waitForStarted(5000) || WaitForSingleObject(ready_, 5000) != WAIT_OBJECT_0) {
        restore(); fail(tr("Процесс восстановления не готов. Панель Windows сохранена.")); return;
    }
    settings_.setValue("recovery/autoHide", (original_ & ABS_AUTOHIDE) != 0);
    settings_.setValue("recovery/pending", true); settings_.sync();
    if (settings_.status() != QSettings::NoError) { restore(); fail(tr("Не удалось сохранить журнал восстановления.")); return; }
    active_ = true;
    if (!setAutoHide(true)) { restore(); fail(tr("Windows отклонила автоскрытие панели.")); return; }
    for (auto& bar : bars_) {
        bar->native = std::make_unique<DesktopBar>(reinterpret_cast<HWND>(bar->view->winId()));
        if (!bar->native->registered() || !bar->native->position(56, bar->view->devicePixelRatio())) {
            restore(); for (auto& item : bars_) item->native.reset();
            fail(tr("Не удалось зарезервировать место панели.")); return;
        }
    }
    status_ = tr("Панель Umbrella работает. Ctrl+Alt+U — аварийный выход."); emit changed();
}
bool ShellController::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    auto* msg = static_cast<MSG*>(message);
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (msg->message == taskbarCreated && enabled_) QTimer::singleShot(0, this, [this] { rebuild(); });
    if (msg->message == WM_HOTKEY && msg->wParam == 1) quit();
    if (msg->message == WM_QUERYENDSESSION || msg->message == WM_ENDSESSION) restore();
    if (msg->message == barCallback && msg->wParam == ABN_FULLSCREENAPP) {
        SetWindowPos(msg->hwnd, msg->lParam ? HWND_BOTTOM : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (msg->message == barCallback && msg->wParam == ABN_POSCHANGED) {
        for (auto& bar : bars_) if (bar->native && reinterpret_cast<HWND>(bar->view->winId()) == msg->hwnd)
            bar->native->position(56, bar->view->devicePixelRatio());
    }
    return false;
}
