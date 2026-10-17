#include "DesktopBar.h"
#include "TaskbarVisibility.h"
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <cmath>
namespace umbrella::platform {
constexpr UINT barCallback = WM_APP + 31;
struct DesktopBar::Impl {
    HWND window_;
    bool registered_ = false;
    bool positioned_ = false;
};
bool requestTaskbarRecovery() {
    HANDLE stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\UmbrellaShell.Recovery");
    if (!stop) return false;
    const bool result = SetEvent(stop) != FALSE;
    CloseHandle(stop);
    return result;
}
std::uint64_t workingSetBytes() {
    PROCESS_MEMORY_COUNTERS memory{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) return 0;
    return memory.WorkingSetSize;
}
unsigned taskbarState() {
    APPBARDATA data{sizeof(data)};
    return static_cast<unsigned>(SHAppBarMessage(ABM_GETSTATE, &data));
}
bool setAutoHide(bool enabled) {
    APPBARDATA data{sizeof(data)};
    data.lParam = static_cast<LPARAM>((taskbarState() & ~ABS_AUTOHIDE) | (enabled ? ABS_AUTOHIDE : 0));
    SHAppBarMessage(ABM_SETSTATE, &data);
    return ((taskbarState() & ABS_AUTOHIDE) != 0) == enabled;
}
int guardTaskbar(std::uint32_t parent, unsigned original, const std::wstring& ready, const std::wstring& stop, bool (*restore)(bool), bool suppress) {
    HANDLE instance = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\UmbrellaShell.DesktopBar");
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, parent);
    HANDLE readyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready.c_str());
    HANDLE stopEvent = OpenEventW(SYNCHRONIZE, FALSE, stop.c_str());
    if (!instance || !process || !readyEvent || !stopEvent) {
        if (instance) CloseHandle(instance);
        if (process) CloseHandle(process);
        if (readyEvent) CloseHandle(readyEvent);
        if (stopEvent) CloseHandle(stopEvent);
        return 1;
    }
    TaskbarVisibility visibility;
    HANDLE arm = nullptr, applied = nullptr;
    if (suppress) {
        arm = OpenEventW(SYNCHRONIZE, FALSE, (ready + L".arm").c_str());
        applied = OpenEventW(EVENT_MODIFY_STATE, FALSE, (ready + L".applied").c_str());
        if (!arm || !applied || !visibility.install()) {
            if (arm) CloseHandle(arm);
            if (applied) CloseHandle(applied);
            CloseHandle(stopEvent); CloseHandle(readyEvent); CloseHandle(process); CloseHandle(instance);
            return 1;
        }
    }
    HANDLE waits[]{process, stopEvent, arm};
    if (!SetEvent(readyEvent)) {
        if (arm) CloseHandle(arm);
        if (applied) CloseHandle(applied);
        CloseHandle(stopEvent); CloseHandle(readyEvent); CloseHandle(process); CloseHandle(instance);
        return 1;
    }
    DWORD result = WAIT_FAILED;
    bool armed = false;
    for (;;) {
        result = suppress ? MsgWaitForMultipleObjectsEx(armed ? 2 : 3, waits, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE)
                          : WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0 || result == WAIT_OBJECT_0 + 1 || result == WAIT_FAILED) break;
        if (suppress && !armed && result == WAIT_OBJECT_0 + 2) {
            armed = true;
            if (!visibility.hide() || !SetEvent(applied)) { result = WAIT_FAILED; break; }
        } else if (suppress) {
            MSG message{};
            for (int batch = 0; batch < 256 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++batch) {
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (!visibility.healthy()) { result = WAIT_FAILED; break; }
        }
    }
    const bool shown = visibility.restore();
    if (arm) CloseHandle(arm);
    if (applied) CloseHandle(applied);
    const bool restored = restore((original & ABS_AUTOHIDE) != 0);
    CloseHandle(stopEvent); CloseHandle(readyEvent); CloseHandle(process); CloseHandle(instance);
    return result == WAIT_FAILED || !restored || !shown ? 1 : 0;
}
namespace {
constexpr auto runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
std::wstring command(const std::wstring& executable) { return L"\"" + executable + L"\""; }
}
bool startupEnabled(const std::wstring& executable) {
    wchar_t value[32768]{}; DWORD size = sizeof(value); DWORD type = 0;
    return RegGetValueW(HKEY_CURRENT_USER, runKey, L"UmbrellaShell", RRF_RT_REG_SZ,
                        &type, value, &size) == ERROR_SUCCESS && value == command(executable);
}
bool setStartup(bool enabled, const std::wstring& executable) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS status;
    if (enabled) {
        const auto value = command(executable);
        status = RegSetValueExW(key, L"UmbrellaShell", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    } else {
        // Never delete a value owned by a different installation.
        status = startupEnabled(executable) ? RegDeleteValueW(key, L"UmbrellaShell") : ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}
DesktopBar::DesktopBar(std::uintptr_t window) : impl_(std::make_unique<Impl>()) {
    impl_->window_ = reinterpret_cast<HWND>(window);
    APPBARDATA data{sizeof(data)}; data.hWnd = impl_->window_; data.uCallbackMessage = barCallback;
    impl_->registered_ = SHAppBarMessage(ABM_NEW, &data) != 0;
    if (impl_->registered_ && !SetPropW(impl_->window_, L"UmbrellaShell.AppBar", reinterpret_cast<HANDLE>(1))) {
        SHAppBarMessage(ABM_REMOVE, &data); impl_->registered_ = false;
    }
}
bool DesktopBar::registered() const { return impl_->registered_; }
DesktopBar::~DesktopBar() {
    if (impl_->registered_) {
        RemovePropW(impl_->window_, L"UmbrellaShell.AppBar");
        APPBARDATA data{sizeof(data)}; data.hWnd = impl_->window_; SHAppBarMessage(ABM_REMOVE, &data);
    }
}
bool DesktopBar::position(int logicalHeight, double scale) {
    auto& state = *impl_;
    MONITORINFO monitor{sizeof(monitor)};
    if (!state.registered_ || !GetMonitorInfoW(MonitorFromWindow(state.window_, MONITOR_DEFAULTTONEAREST), &monitor)) return false;
    APPBARDATA data{sizeof(data)}; data.hWnd = state.window_; data.uEdge = ABE_BOTTOM; data.rc = monitor.rcMonitor;
    const LONG height = static_cast<LONG>(std::lround(logicalHeight * scale));
    SHAppBarMessage(ABM_QUERYPOS, &data);
    data.rc.top = data.rc.bottom - height;
    RECT current{};
    if (GetWindowRect(state.window_, &current) && EqualRect(&current, &data.rc) && state.positioned_) return true;
    SHAppBarMessage(ABM_SETPOS, &data);
    state.positioned_ = true;
    return SetWindowPos(state.window_, HWND_TOPMOST, data.rc.left, data.rc.top,
        data.rc.right - data.rc.left, data.rc.bottom - data.rc.top, SWP_NOACTIVATE) != FALSE;
}
}
