#include "TaskbarRuntime.h"
#include "TaskbarWindows.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QSettings>
#include <QTimer>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QDateTime>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <Windows.h>
#include <array>
#include <algorithm>
#include <vector>

namespace umbrella::platform {
namespace {
bool panel(HWND window) {
    wchar_t name[64]{};
    return GetClassNameW(window, name, 64) &&
        (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd"));
}
QString device(HWND window) {
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    return GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)
        ? QString::fromWCharArray(info.szDevice) : QString();
}
bool explorer(HWND window) {
    DWORD pid{}; GetWindowThreadProcessId(window, &pid);
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> path{}; DWORD length = static_cast<DWORD>(path.size());
    std::array<wchar_t, MAX_PATH> system{};
    const auto size = GetWindowsDirectoryW(system.data(), static_cast<UINT>(system.size()));
    const bool nativeOk = size && size < system.size() && QueryFullProcessImageNameW(process, 0, path.data(), &length)
        && !QString::compare(QDir::fromNativeSeparators(QString::fromWCharArray(path.data())), QDir::fromNativeSeparators(QString::fromWCharArray(system.data())) + "/explorer.exe", Qt::CaseInsensitive);
    CloseHandle(process); return nativeOk;
}
}
struct TaskbarRuntime::Impl {
    TaskbarRuntime* owner;
    HANDLE instance{};
    bool acquired = false;
    HWINEVENTHOOK objects{}, locations{};
    QTimer coalesce;
    std::function<bool()> ready;
    QString signature;
    unsigned attempts = 0;
    std::vector<HWND> tracked;
    QElapsedTimer readinessWindow;
    unsigned readinessChecks = 0;
    QString waitingReason;
    bool applying = false, checkAgain = false;
    inline static Impl* active{};
    explicit Impl(TaskbarRuntime* value) : owner(value) {
        instance = CreateMutexW(nullptr, TRUE, L"Local\\UmbrellaShell.SettingsOwner.1");
        acquired = instance && GetLastError() != ERROR_ALREADY_EXISTS;
        coalesce.setSingleShot(true);
        QObject::connect(&coalesce, &QTimer::timeout, owner, [this] { check(); });
    }
    void queue() { if (!coalesce.isActive() || coalesce.remainingTime() > 0) coalesce.start(0); }
    void waitForReadiness(const QString& reason) {
        signature.clear();
        if (waitingReason != reason) { waitingReason = reason; TaskbarRuntime::writeRestoreReport(false, reason); }
        if (!readinessWindow.isValid()) readinessWindow.start();
        // Events are primary. Cover a lost/out-of-order creation event during
        // startup/recreation only, with bounded backoff and no idle polling.
        if (readinessWindow.elapsed() < 30000 && readinessChecks < 16) {
            const unsigned shift = (std::min)(readinessChecks++, 4u);
            coalesce.start(static_cast<int>((std::min)(100u << shift, 2000u)));
        }
    }
    void check() {
        if (applying) { checkAgain = true; return; }
        const auto screens = TaskbarRuntime::monitors();
        if (screens.isEmpty()) { waitForReadiness(QStringLiteral("Ожидание списка активных мониторов.")); return; }
        struct State { QStringList parts, devices; std::vector<HWND> windows; bool valid = true; } state;
        const auto inspect = [](HWND window, LPARAM context) -> BOOL {
            if (!panel(window)) return TRUE;
            auto& state = *reinterpret_cast<State*>(context);
            RECT rect{}; DWORD pid{}; GetWindowThreadProcessId(window, &pid);
            // A ready Win11 XAML taskbar need not expose a child bridge HWND
            // (for example after Task View). The native module validates the
            // actual XamlRoot/controls and acknowledges application itself.
            if (!explorer(window) || !GetWindowRect(window, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) state.valid = false;
            state.devices.append(device(window));
            state.windows.push_back(window);
            state.parts.append(QString::number(reinterpret_cast<quintptr>(window), 16) + ":" + QString::number(pid));
            return state.parts.size() <= 16;
        };
        for (const auto window : taskbarWindows()) if (!inspect(window, reinterpret_cast<LPARAM>(&state))) break;
        tracked = state.windows;
        if (!state.valid || state.parts.isEmpty() || state.parts.size() > 16 || !FindWindowW(L"Shell_TrayWnd", nullptr)) { waitForReadiness(QStringLiteral("Ожидание окон Explorer.")); return; }
        for (const auto& value : screens) {
            const auto screen = value.toMap();
            if (!state.devices.contains(screen["device"].toString())) { waitForReadiness(QStringLiteral("Ожидание панели активного монитора.")); return; }
            state.parts.append(screen["id"].toString() + screen["device"].toString());
        }
        std::sort(state.parts.begin(), state.parts.end());
        const auto next = state.parts.join('|');
        if (signature != next) { signature = next; attempts = 0; }
        if (attempts >= 3) return;
        ++attempts;
        applying = true;
        const bool handled = !ready || ready();
        applying = false;
        if (checkAgain) { checkAgain = false; queue(); }
        if (handled) { attempts = 3; readinessWindow.invalidate(); readinessChecks = 0; waitingReason.clear(); return; }
        // A bridge can exist before its XAML provider accepts the handshake.
        // Retry only a failed readiness attempt, bounded; no timer runs at idle.
        if (attempts < 3) coalesce.start(static_cast<int>(attempts) * 200);
    }
    static void CALLBACK changed(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD) {
        if (!active || object != OBJID_WINDOW || child != CHILDID_SELF) return;
        if (panel(window) || panel(GetAncestor(window, GA_ROOT)) ||
            (event == EVENT_OBJECT_DESTROY && std::find(active->tracked.begin(), active->tracked.end(), window) != active->tracked.end())) active->queue();
    }
};
TaskbarRuntime::TaskbarRuntime(QObject* parent) : QObject(parent), impl_(std::make_unique<Impl>(this)) {}
TaskbarRuntime::~TaskbarRuntime() {
    QCoreApplication::instance()->removeNativeEventFilter(this);
    if (Impl::active == impl_.get()) Impl::active = nullptr;
    if (impl_->objects) UnhookWinEvent(impl_->objects);
    if (impl_->locations) UnhookWinEvent(impl_->locations);
    if (impl_->acquired) ReleaseMutex(impl_->instance);
    if (impl_->instance) CloseHandle(impl_->instance);
}
bool TaskbarRuntime::ownsInstance() const { return impl_->acquired; }
bool TaskbarRuntime::start(std::function<bool()> ready) {
    if (!ownsInstance()) return false;
    impl_->ready = std::move(ready); Impl::active = impl_.get();
    impl_->objects = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, nullptr, Impl::changed, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    impl_->locations = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, Impl::changed, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    QCoreApplication::instance()->installNativeEventFilter(this);
    impl_->queue();
    return impl_->objects && impl_->locations;
}
bool TaskbarRuntime::nativeEventFilter(const QByteArray&, void* message, qintptr*) {
    const auto* event = static_cast<MSG*>(message);
    if (event && (event->message == RegisterWindowMessageW(L"TaskbarCreated") || event->message == WM_DISPLAYCHANGE)) {
        impl_->signature.clear(); impl_->readinessWindow.restart(); impl_->readinessChecks = 0; impl_->queue();
    }
    return false;
}
QVariantList TaskbarRuntime::monitors() {
    UINT32 paths{}, modes{};
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &paths, &modes) != ERROR_SUCCESS || paths > 16 || modes > 128) return {};
    std::vector<DISPLAYCONFIG_PATH_INFO> connections(paths);
    std::vector<DISPLAYCONFIG_MODE_INFO> settings(modes);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &paths, connections.data(), &modes, settings.data(), nullptr) != ERROR_SUCCESS) return {};
    QVariantList result;
    for (UINT32 i = 0; i < paths; ++i) {
        const auto& path = connections[i];
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header = {DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(target), path.targetInfo.adapterId, path.targetInfo.id};
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source), path.sourceInfo.adapterId, path.sourceInfo.id};
        if (DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS || DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS || !target.monitorDevicePath[0]) continue;
        const auto id = QString::fromLatin1(QCryptographicHash::hash(QString::fromWCharArray(target.monitorDevicePath).toLower().toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto name = QString::fromWCharArray(source.viewGdiDeviceName);
        bool primary = false;
        int width = 0;
        struct Find { QString name; bool* primary; int* width; } find{name, &primary, &width};
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
            auto& find = *reinterpret_cast<Find*>(data); MONITORINFOEXW info{}; info.cbSize = sizeof(info);
            if (GetMonitorInfoW(monitor, &info) && find.name == QString::fromWCharArray(info.szDevice)) {
                *find.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                *find.width = info.rcMonitor.right - info.rcMonitor.left;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&find));
        result.append(QVariantMap{{"id", id}, {"device", name}, {"primary", primary}, {"width", width}, {"label", QString::fromWCharArray(target.monitorFriendlyDeviceName)}});
    }
    return result;
}
bool TaskbarRuntime::setLoginResume(bool enabled) {
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);
    if (enabled) run.setValue("UmbrellaShell", QStringLiteral("\"%1\" --resume-settings").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else run.remove("UmbrellaShell");
    run.sync(); return run.status() == QSettings::NoError;
}
QStringList TaskbarRuntime::panelOwners() {
    QStringList result,devices;
    const auto windows=taskbarWindows();
    if(windows.empty() || windows.size()>16)return {};
    for(const auto window:windows) {
        if(!explorer(window))return {};
        DWORD pid{};if(!GetWindowThreadProcessId(window,&pid) || !pid)return {};
        struct Process {HANDLE value;~Process(){if(value)CloseHandle(value);}} process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)};
        FILETIME created{},exited{},kernel{},user{};
        const auto name=device(window);
        if(!process.value || name.isEmpty() || !GetProcessTimes(process.value,&created,&exited,&kernel,&user))return {};
        devices.append(name);
        result.append(name+':'+QString::number(reinterpret_cast<quintptr>(window),16)+':'+QString::number(pid)+':'+
            QString::number(created.dwHighDateTime)+':'+QString::number(created.dwLowDateTime));
    }
    const auto screens=monitors();if(screens.isEmpty())return {};
    for(const auto& entry:screens) {
        const auto monitor=entry.toMap();
        if(!devices.contains(monitor["device"].toString()))return {};
        result.append(monitor["id"].toString()+':'+monitor["device"].toString());
    }
    result.sort();return result;
}
QString TaskbarRuntime::reportPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/startup-status.json";
}
bool TaskbarRuntime::writeRestoreReport(bool succeeded, const QString& status) {
    QJsonArray panels;
    const auto inspect = [](HWND window, LPARAM context) -> BOOL {
        if (!panel(window)) return TRUE;
        RECT rect{}; DWORD pid{}; GetWindowThreadProcessId(window, &pid);
        BYTE alpha = 255; COLORREF key{}; DWORD flags{};
        GetLayeredWindowAttributes(window, &key, &alpha, &flags);
        if (GetWindowRect(window, &rect)) reinterpret_cast<QJsonArray*>(context)->append(QJsonObject{
            {"device", device(window)}, {"explorerPid", static_cast<int>(pid)}, {"x", static_cast<int>(rect.left)}, {"y", static_cast<int>(rect.top)},
            {"width", static_cast<int>(rect.right-rect.left)}, {"height", static_cast<int>(rect.bottom-rect.top)},
            {"dpi", static_cast<int>(GetDpiForWindow(window))}, {"alpha", static_cast<int>(alpha)}});
        return reinterpret_cast<QJsonArray*>(context)->size() <= 16;
    };
    for (const auto window : taskbarWindows()) if (!inspect(window, reinterpret_cast<LPARAM>(&panels))) break;
    const auto path = reportPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"ownerPid", QCoreApplication::applicationPid()}, {"succeeded", succeeded}, {"status", status},
        {"monitors", QJsonArray::fromVariantList(monitors())}, {"panels", panels}}).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}
