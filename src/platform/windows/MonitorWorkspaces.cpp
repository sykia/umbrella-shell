#include "MonitorWorkspaces.h"
#include "core/PanelConfiguration.h"
#include <QTimer>
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <map>
#include <vector>
namespace umbrella::platform {
namespace {
constexpr unsigned maxWindows = 256, maxMonitors = 16, signature = 0x554d4257;
struct Identity { UINT_PTR hwnd; DWORD pid; FILETIME created; UINT show; };
struct Snapshot { unsigned count; Identity windows[maxWindows]; };
struct Shared { unsigned magic; volatile LONG published; Snapshot snapshots[2]; };
struct Close { void operator()(void* value) const { if (value) CloseHandle(value); } };
using Handle = std::unique_ptr<void, Close>;
struct Unlock { HANDLE handle; ~Unlock() { ReleaseMutex(handle); } };
bool lock(HANDLE value) { const auto result = WaitForSingleObject(value, 1000); return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED; }
bool identity(HWND hwnd, Identity& out) {
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!process || !GetProcessTimes(process.get(), &created, &exited, &kernel, &user)) return false;
    WINDOWPLACEMENT placement{sizeof(placement)};
    if (!GetWindowPlacement(hwnd, &placement)) return false;
    out = {reinterpret_cast<UINT_PTR>(hwnd), pid, created, placement.showCmd}; return true;
}
bool same(const Identity& stored) {
    Identity now{};
    return identity(reinterpret_cast<HWND>(stored.hwnd), now) && now.pid == stored.pid && CompareFileTime(&now.created, &stored.created) == 0;
}
bool eligible(HWND hwnd, bool scopedFixture) {
    const auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const auto extended = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // Taskbar application windows can draw their own title bar.
    // Keep owned dialogs outside independent groups: hiding their owner can
    // implicitly hide them, including when they are on another monitor.
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || (style & WS_CHILD) ||
        (extended & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) || (!(style & WS_CAPTION) && !(extended & WS_EX_APPWINDOW))) return false;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return false;
    // Elevated/system windows are outside this manager's scope.
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)); HANDLE tokenValue = nullptr;
    if (!process || !OpenProcessToken(process.get(), TOKEN_QUERY, &tokenValue)) return false;
    Handle token(tokenValue); TOKEN_ELEVATION elevation{}; DWORD length = 0;
    if (!GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &length) || (elevation.TokenIsElevated && !scopedFixture)) return false;
    return true;
}
bool show(const Identity& stored) {
    if (!same(stored)) return true;
    const auto hwnd = reinterpret_cast<HWND>(stored.hwnd);
    ShowWindow(hwnd, stored.show == SW_SHOWMINIMIZED || stored.show == SW_MINIMIZE || stored.show == SW_SHOWMINNOACTIVE ? SW_SHOWMINNOACTIVE : SW_SHOWNA);
    return IsWindowVisible(hwnd) != FALSE;
}
}
struct WorkspaceSnapshot::Impl {
    Handle mapping, mutex;
    Shared* view = nullptr;
    ~Impl() { if (view) UnmapViewOfFile(view); }
    bool publish(const std::vector<Identity>& windows) {
        if (!view || windows.size() > maxWindows || !lock(mutex.get())) return false;
        Unlock unlock{mutex.get()};
        const LONG next = 1 - InterlockedCompareExchange(&view->published, 0, 0);
        auto& snapshot = view->snapshots[next];
        snapshot.count = static_cast<unsigned>(windows.size());
        std::copy(windows.begin(), windows.end(), snapshot.windows);
        MemoryBarrier(); InterlockedExchange(&view->published, next); return true;
    }
};
WorkspaceSnapshot::WorkspaceSnapshot() : impl(std::make_unique<Impl>()) {}
WorkspaceSnapshot::~WorkspaceSnapshot() = default;
bool WorkspaceSnapshot::open(const std::wstring& name, bool create) {
    impl = std::make_unique<Impl>();
    if (name.empty()) return false;
    impl->mapping.reset(create ? CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(sizeof(Shared)), name.c_str())
                               : OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name.c_str()));
    const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;
    impl->mutex.reset(create ? CreateMutexW(nullptr, FALSE, (name + L".mutex").c_str())
                             : OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name + L".mutex").c_str()));
    if (!impl->mapping || !impl->mutex) return false;
    impl->view = static_cast<Shared*>(MapViewOfFile(impl->mapping.get(), FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!impl->view) return false;
    if (create && !existed) { ZeroMemory(impl->view, sizeof(Shared)); impl->view->magic = signature; }
    return impl->view->magic == signature && impl->view->published >= 0 && impl->view->published <= 1;
}
bool WorkspaceSnapshot::restore() {
    if (!impl->view) return true;
    if (!lock(impl->mutex.get())) return false;
    Snapshot copy{};
    { Unlock unlock{impl->mutex.get()}; const LONG index = InterlockedCompareExchange(&impl->view->published, 0, 0);
      if (impl->view->magic != signature || index < 0 || index > 1 || impl->view->snapshots[index].count > maxWindows) return false;
      copy = impl->view->snapshots[index]; }
    bool result = true;
    for (unsigned i = 0; i < copy.count; ++i) result = show(copy.windows[i]) && result;
    return result;
}
struct MonitorWorkspaces::Impl {
    struct Window { Identity id; UINT_PTR monitor; unsigned space; bool hidden = false; };
    QTimer timer;
    WorkspaceSnapshot snapshot;
    std::vector<Window> windows;
    std::map<UINT_PTR, unsigned> active;
    bool enabled = false;
    DWORD scope = 0;
    unsigned count = 5;
    QString snapshotName;
    QVariantList fingerprint;
    bool publish() {
        std::vector<Identity> hidden;
        for (const auto& window : windows) if (window.hidden) hidden.push_back(window.id);
        return snapshot.impl->publish(hidden);
    }
};
MonitorWorkspaces::MonitorWorkspaces(QObject* parent, quint32 scopeProcess) : QObject(parent), impl_(std::make_unique<Impl>()) {
    impl_->scope = scopeProcess;
    impl_->timer.setInterval(500); impl_->timer.setTimerType(Qt::CoarseTimer);
    connect(&impl_->timer, &QTimer::timeout, this, &MonitorWorkspaces::refresh);
}
MonitorWorkspaces::~MonitorWorkspaces() { stop(); }
bool MonitorWorkspaces::running() const { return impl_->enabled; }
bool MonitorWorkspaces::setCount(int count) {
    if (!umbrella::core::validWorkspaceCount(count)) return false;
    if (impl_->count == static_cast<unsigned>(count)) return true;
    const bool restart = impl_->enabled;
    const auto name = impl_->snapshotName;
    // Never leave windows stranded in a removed group. Return all before regrouping.
    if (!stop()) return false;
    impl_->count = static_cast<unsigned>(count);
    return !restart || start(name);
}
bool MonitorWorkspaces::start(const QString& snapshot) {
    if (!stop() || !impl_->snapshot.open(snapshot.toStdWString(), false)) return false;
    impl_->snapshotName = snapshot;
    impl_->enabled = true; refresh(); if (impl_->enabled) impl_->timer.start(); emit changed(); return impl_->enabled;
}
bool MonitorWorkspaces::stop() {
    impl_->timer.stop(); impl_->enabled = false;
    const bool restored = impl_->snapshot.restore();
    if (restored) { impl_->windows.clear(); impl_->active.clear(); if (impl_->snapshot.impl->view) impl_->snapshot.impl->publish({}); }
    emit changed(); return restored;
}
void MonitorWorkspaces::refresh() {
    if (!impl_->enabled) return;
    std::erase_if(impl_->windows, [](const Impl::Window& window) { return !same(window.id); });
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        auto* self = reinterpret_cast<Impl*>(data);
        if (self->active.size() < maxMonitors) self->active.try_emplace(reinterpret_cast<UINT_PTR>(monitor), 0);
        return TRUE;
    }, reinterpret_cast<LPARAM>(impl_.get()));
    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        auto* self = reinterpret_cast<Impl*>(data);
        auto found = std::find_if(self->windows.begin(), self->windows.end(), [hwnd](const auto& value) { return value.id.hwnd == reinterpret_cast<UINT_PTR>(hwnd); });
        if (found != self->windows.end() && found->hidden) { if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE); return TRUE; }
        DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
        if (self->scope && pid != self->scope) return TRUE;
        if (!eligible(hwnd, self->scope != 0)) return TRUE;
        const auto monitor = reinterpret_cast<UINT_PTR>(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
        const auto active = self->active.find(monitor); if (active == self->active.end()) return TRUE;
        if (found != self->windows.end()) {
            if (found->monitor != monitor) { found->monitor = monitor; found->space = active->second; }
            identity(hwnd, found->id);
        } else if (self->windows.size() < maxWindows) {
            Identity id{}; if (identity(hwnd, id)) self->windows.push_back({id, monitor, active->second, false});
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(impl_.get()));
    QVariantList fingerprint;
    for (const auto& [monitor, active] : impl_->active) {
        fingerprint << QVariant::fromValue<qulonglong>(monitor) << active;
        for (unsigned space = 0; space < impl_->count; ++space)
            fingerprint << static_cast<int>(std::count_if(impl_->windows.begin(), impl_->windows.end(), [=](const auto& window) { return window.monitor == monitor && window.space == space; }));
    }
    if (impl_->fingerprint != fingerprint) { impl_->fingerprint = fingerprint; emit changed(); }
}
QVariantList MonitorWorkspaces::model(quintptr panel) const {
    QVariantList result; if (!impl_->enabled) return result;
    const auto monitor = reinterpret_cast<UINT_PTR>(MonitorFromWindow(reinterpret_cast<HWND>(panel), MONITOR_DEFAULTTONEAREST));
    const auto found = impl_->active.find(monitor); if (found == impl_->active.end()) return result;
    for (unsigned space = 0; space < impl_->count; ++space) {
        const auto count = std::count_if(impl_->windows.begin(), impl_->windows.end(), [=](const auto& window) { return window.monitor == monitor && window.space == space; });
        result << QVariantMap{{"index", space}, {"active", found->second == space}, {"occupied", count > 0}};
    }
    return result;
}
bool MonitorWorkspaces::select(quintptr panel, int index) {
    DWORD pid = 0; GetWindowThreadProcessId(reinterpret_cast<HWND>(panel), &pid);
    if (!impl_->enabled || index < 0 || index >= static_cast<int>(impl_->count) || pid != GetCurrentProcessId() ||
        !GetPropW(reinterpret_cast<HWND>(panel), L"UmbrellaShell.AppBar")) return false;
    refresh(); const auto monitor = reinterpret_cast<UINT_PTR>(MonitorFromWindow(reinterpret_cast<HWND>(panel), MONITOR_DEFAULTTONEAREST));
    auto active = impl_->active.find(monitor); if (active == impl_->active.end()) return false;
    if (active->second == static_cast<unsigned>(index)) return true;
    for (auto& window : impl_->windows) if (window.monitor == monitor && window.space != static_cast<unsigned>(index) && !window.hidden && IsWindowVisible(reinterpret_cast<HWND>(window.id.hwnd))) {
        identity(reinterpret_cast<HWND>(window.id.hwnd), window.id); window.hidden = true;
    }
    // Publish intent before hiding: the guardian can recover after any following instruction.
    if (!impl_->publish()) { for (auto& window : impl_->windows) window.hidden = false; stop(); return false; }
    // Commit visibility together. Do not move/resize/activate windows or change
    // their stacking order; minimized and maximized placement stays intact.
    std::vector<Impl::Window*> changes;
    for (auto& window : impl_->windows)
        if (window.monitor == monitor && window.hidden && same(window.id)) changes.push_back(&window);
    bool ok = true;
    if (!changes.empty()) {
        auto batch = BeginDeferWindowPos(static_cast<int>(changes.size()));
        ok = batch != nullptr;
        for (const auto* window : changes) {
            if (!ok) break;
            const UINT visibility = window->space == static_cast<unsigned>(index) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
            batch = DeferWindowPos(batch, reinterpret_cast<HWND>(window->id.hwnd), nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER | visibility);
            ok = batch != nullptr;
        }
        // DeferWindowPos frees the batch on failure: never end an invalid batch.
        if (ok) ok = EndDeferWindowPos(batch) != FALSE;
        if (ok) for (auto* window : changes) {
            if (!same(window->id)) continue; // Closed during the operation.
            const bool visible = IsWindowVisible(reinterpret_cast<HWND>(window->id.hwnd)) != FALSE;
            if (visible != (window->space == static_cast<unsigned>(index))) ok = false;
            else if (visible) window->hidden = false;
        }
    }
    if (!ok) { stop(); emit failed(tr("Не удалось безопасно переключить группу окон. Проверьте восстановление окон.")); return false; }
    if (!impl_->publish()) { stop(); return false; }
    active->second = static_cast<unsigned>(index); refresh(); emit changed(); return true;
}
}
