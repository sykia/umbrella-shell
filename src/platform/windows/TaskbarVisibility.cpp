#include "TaskbarVisibility.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <vector>
#include <sstream>
namespace umbrella::platform {
namespace {
struct Window { HWND hwnd; DWORD pid; FILETIME created; bool visible; };
bool explorerWindow(HWND hwnd, Window* snapshot = nullptr) {
    wchar_t name[64]{};
    if (!GetClassNameW(hwnd, name, 64) ||
        (wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd"))) return false;
    DWORD pid{}; GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> path{}; DWORD size = static_cast<DWORD>(path.size());
    wchar_t windows[MAX_PATH]{}; const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    FILETIME created{}, exited{}, kernel{}, user{};
    const bool queried = QueryFullProcessImageNameW(process, 0, path.data(), &size) &&
        GetProcessTimes(process, &created, &exited, &kernel, &user);
    CloseHandle(process);
    if (!queried || !length || length >= MAX_PATH ||
        _wcsicmp(path.data(), (std::wstring(windows) + L"\\explorer.exe").c_str())) return false;
    if (snapshot) *snapshot = {hwnd, pid, created, IsWindowVisible(hwnd) != FALSE};
    return true;
}
bool sameWindow(const Window& window) {
    Window current{};
    return explorerWindow(window.hwnd, &current) && current.pid == window.pid &&
        CompareFileTime(&current.created, &window.created) == 0;
}
std::vector<Window> windows() {
    std::vector<Window> result;
    EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        auto& items = *reinterpret_cast<std::vector<Window>*>(data);
        Window item{};
        if (explorerWindow(hwnd, &item)) items.push_back(item);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}
}
struct TaskbarVisibility::Impl {
    std::vector<Window> hidden;
    HWINEVENTHOOK hook{};
    bool active = false, failed = false;
    static thread_local Impl* current;
    static void CALLBACK event(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD) {
        auto* self = current;
        if (!self || !self->active || object != OBJID_WINDOW || child != CHILDID_SELF || !hwnd) return;
        if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_LOCATIONCHANGE)
            if (!self->suppress(hwnd)) self->failed = true;
    }
    bool suppress(HWND hwnd) {
        Window item{};
        if (!explorerWindow(hwnd, &item)) return true;
        std::erase_if(hidden, [](const Window& previous) { return !sameWindow(previous); });
        const auto found = std::find_if(hidden.begin(), hidden.end(), [hwnd](const Window& value) { return value.hwnd == hwnd; });
        if (found == hidden.end()) {
            // CREATE can precede SHOW. Do not record a not-yet-visible new window as originally hidden.
            if (!item.visible) return true;
            // Bound retained identities even during repeated Explorer restarts.
            if (hidden.size() >= 64) return false;
            hidden.push_back(item); // Record before making any change.
        }
        if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
        return !IsWindowVisible(hwnd);
    }
};
thread_local TaskbarVisibility::Impl* TaskbarVisibility::Impl::current = nullptr;
TaskbarVisibility::TaskbarVisibility() : impl_(std::make_unique<Impl>()) {}
TaskbarVisibility::~TaskbarVisibility() { restore(); }
bool TaskbarVisibility::install() {
    if (Impl::current || impl_->hook) return false;
    Impl::current = impl_.get();
    impl_->hook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
        Impl::event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!impl_->hook) Impl::current = nullptr;
    return impl_->hook != nullptr;
}
bool TaskbarVisibility::healthy() const { return !impl_->failed; }
bool TaskbarVisibility::hide() {
    if (!impl_->hook) return false;
    const auto existing = windows();
    if (existing.empty()) return false;
    impl_->active = true;
    for (const auto& window : existing) if (!impl_->suppress(window.hwnd)) { restore(); return false; }
    return true;
}
bool TaskbarVisibility::restore() {
    impl_->active = false;
    if (impl_->hook) { UnhookWinEvent(impl_->hook); impl_->hook = nullptr; }
    if (Impl::current == impl_.get()) Impl::current = nullptr;
    bool result = true;
    for (const auto& window : impl_->hidden) {
        if (!sameWindow(window) || !window.visible) continue;
        ShowWindow(window.hwnd, SW_SHOWNA);
        result = IsWindowVisible(window.hwnd) != FALSE && result;
    }
    impl_->hidden.clear();
    return result;
}
bool restoreTaskbarWindows() {
    bool result = true;
    for (const auto& window : windows()) {
        ShowWindow(window.hwnd, SW_SHOWNA);
        result = IsWindowVisible(window.hwnd) != FALSE && result;
    }
    return result;
}
std::string taskbarStatusJson() {
    std::ostringstream out; out << "{\"taskbars\":[";
    bool first = true;
    for (const auto& window : windows()) {
        RECT rect{}; GetWindowRect(window.hwnd, &rect);
        if (!first) out << ','; first = false;
        out << "{\"window\":" << reinterpret_cast<std::uintptr_t>(window.hwnd)
            << ",\"visible\":" << (IsWindowVisible(window.hwnd) ? "true" : "false")
            << ",\"left\":" << rect.left << ",\"top\":" << rect.top
            << ",\"right\":" << rect.right << ",\"bottom\":" << rect.bottom << '}';
    }
    return out.str() + "]}";
}
}
