#include "DesktopBar.h"
#include <cmath>
namespace umbrella::platform {
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
int guardTaskbar(DWORD parent, unsigned original, const std::wstring& ready, const std::wstring& stop) {
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
    HANDLE waits[]{process, stopEvent};
    SetEvent(readyEvent);
    const DWORD result = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    const bool restored = setAutoHide((original & ABS_AUTOHIDE) != 0);
    CloseHandle(stopEvent); CloseHandle(readyEvent); CloseHandle(process); CloseHandle(instance);
    return result == WAIT_FAILED || !restored ? 1 : 0;
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
DesktopBar::DesktopBar(HWND window) : window_(window) {
    APPBARDATA data{sizeof(data)}; data.hWnd = window_; data.uCallbackMessage = barCallback;
    registered_ = SHAppBarMessage(ABM_NEW, &data) != 0;
}
DesktopBar::~DesktopBar() {
    if (registered_) { APPBARDATA data{sizeof(data)}; data.hWnd = window_; SHAppBarMessage(ABM_REMOVE, &data); }
}
bool DesktopBar::position(int logicalHeight, double scale) {
    MONITORINFO monitor{sizeof(monitor)};
    if (!registered_ || !GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor)) return false;
    APPBARDATA data{sizeof(data)}; data.hWnd = window_; data.uEdge = ABE_BOTTOM; data.rc = monitor.rcMonitor;
    const LONG height = static_cast<LONG>(std::lround(logicalHeight * scale));
    SHAppBarMessage(ABM_QUERYPOS, &data);
    data.rc.top = data.rc.bottom - height;
    RECT current{};
    if (GetWindowRect(window_, &current) && EqualRect(&current, &data.rc)) {
        // The initial registration still needs ABM_SETPOS to reserve space.
        if (positioned_) return true;
    }
    SHAppBarMessage(ABM_SETPOS, &data);
    positioned_ = true;
    return SetWindowPos(window_, HWND_TOPMOST, data.rc.left, data.rc.top,
        data.rc.right - data.rc.left, data.rc.bottom - data.rc.top, SWP_NOACTIVATE) != FALSE;
}
}
