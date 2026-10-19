#include "TaskbarContextMenu.h"
#include "TaskbarMenuProtocol.h"
#include "TaskbarMenuPatch.h"
#include <CommCtrl.h>
#include <shellapi.h>

namespace {
using namespace umbrella::explorer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
struct State {
    menu_patch::Patch patch;
    std::array<HWND, 16> panels{};
    unsigned count{}, activeCalls{};
    HWND owner{}, primary{};
    DWORD ownerPid{}, thread{};
    HANDLE process{}, stop{};
    bool stopping = false;
    MenuFlyout flyout{nullptr};
    std::array<MenuFlyoutItem, 4> items{nullptr, nullptr, nullptr, nullptr};
    std::array<winrt::event_token, 4> tokens{};
} state;
constexpr UINT_PTR subclassId = menu_protocol::version;
using ShowMenu = void(WINAPI*)(const FrameworkElement&, const void*, const Input::ContextRequestedEventArgs&, UINT64);
LRESULT CALLBACK procedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
void report(HRESULT value) { PostMessageW(state.owner, menu_protocol::status, 0, value); }
struct Call {
    Call() { ++state.activeCalls; }
    ~Call() {
        if (!--state.activeCalls && state.stopping)
            PostMessageW(state.primary, RegisterWindowMessageW(menu_protocol::detach), menu_protocol::version, 0);
    }
};
void closeFlyout() {
    if (state.flyout) state.flyout.Hide();
    for (unsigned i = 0; i < state.items.size(); ++i) {
        if (state.items[i]) state.items[i].Click(state.tokens[i]);
        state.items[i] = nullptr;
    }
    state.flyout = nullptr;
}
void WINAPI showMenu(const FrameworkElement& element, const void* settings,
                     const Input::ContextRequestedEventArgs& args, UINT64 source) noexcept {
    Call call;
    const auto original = reinterpret_cast<ShowMenu>(state.patch.original());
    if (state.stopping || WaitForSingleObject(state.process, 0) != WAIT_TIMEOUT) {
        if (original) original(element, settings, args, source);
        return;
    }
    try {
        closeFlyout();
        state.flyout = MenuFlyout();
        const bool russian = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_RUSSIAN;
        const wchar_t* labels[]{L"Umbrella", russian ? L"Диспетчер задач" : L"Task Manager",
                               russian ? L"Параметры панели задач" : L"Taskbar settings",
                               russian ? L"Завершить Umbrella" : L"Quit Umbrella"};
        for (unsigned i = 0; i < state.items.size(); ++i) {
            state.items[i] = MenuFlyoutItem();
            state.items[i].Text(labels[i]);
            state.tokens[i] = state.items[i].Click([command = i + 1](auto&&, auto&&) {
                Call callback;
                if (state.stopping || WaitForSingleObject(state.process, 0) != WAIT_TIMEOUT) return;
                AllowSetForegroundWindow(state.ownerPid);
                PostMessageW(state.owner, menu_protocol::action, command, 0);
            });
            state.flyout.Items().Append(state.items[i]);
        }
        winrt::Windows::Foundation::Point point{};
        Primitives::FlyoutShowOptions options;
        if (args.TryGetPosition(element, point)) options.Position(point);
        state.flyout.ShowAt(element, options);
        args.Handled(true);
        report(S_OK);
    } catch (const winrt::hresult_error& error) {
        report(error.code());
        if (original) original(element, settings, args, source);
    } catch (...) {
        report(E_FAIL);
        if (original) original(element, settings, args, source);
    }
}
bool restore() {
    state.stopping = true;
    if (state.activeCalls) return false;
    try { closeFlyout(); } catch (...) { report(E_FAIL); return false; }
    if (!state.patch.restore()) { report(E_FAIL); return false; }
    for (unsigned i = 0; i < state.count; ++i) {
        RemovePropW(state.panels[i], menu_protocol::readyProperty);
        RemoveWindowSubclass(state.panels[i], procedure, subclassId);
    }
    state.count = 0;
    if (state.stop) SetEvent(state.stop);
    if (state.stop) CloseHandle(state.stop);
    if (state.process) CloseHandle(state.process);
    state.stop = nullptr; state.process = nullptr;
    PostMessageW(state.owner, menu_protocol::detached, 0, 0);
    return true;
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if ((message == RegisterWindowMessageW(menu_protocol::detach) && wp == menu_protocol::version) || message == WM_NCDESTROY) {
        restore();
    }
    return DefSubclassProc(window, message, wp, lp);
}
bool ownerIsTrusted(HWND window, HANDLE& process, DWORD& pid) {
    wchar_t name[80]{};
    if (!GetClassNameW(window, name, 80) || wcscmp(name, menu_protocol::ownerClass)) return false;
    GetWindowThreadProcessId(window, &pid);
    process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> path{}, expected{};
    DWORD size = static_cast<DWORD>(path.size());
    HMODULE self{};
    bool ok = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ownerIsTrusted), &self) &&
        GetModuleFileNameW(self, expected.data(), static_cast<DWORD>(expected.size())) &&
        QueryFullProcessImageNameW(process, 0, path.data(), &size);
    if (ok) {
        auto* end = wcsrchr(expected.data(), L'\\');
        if (!end) ok = false;
        else { *(end + 1) = 0; ok = !wcscat_s(expected.data(), expected.size(), L"UmbrellaMenuHost.exe") && !_wcsicmp(path.data(), expected.data()); }
    }
    if (!ok) { CloseHandle(process); process = nullptr; }
    return ok;
}
struct Watch { HANDLE process{}, stop{}; HWND window{}; HMODULE module{}; };
DWORD WINAPI watch(void* parameter) {
    const auto value = *static_cast<Watch*>(parameter);
    delete static_cast<Watch*>(parameter);
    HANDLE waits[]{value.stop, value.process};
    const auto reason = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    DWORD_PTR result{};
    const UINT message = reason == WAIT_OBJECT_0 + 1 ? RegisterWindowMessageW(menu_protocol::detach) : WM_NULL;
    bool returned = reason != WAIT_FAILED && SendMessageTimeoutW(value.window, message, menu_protocol::version, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result);
    if (returned && reason == WAIT_OBJECT_0 + 1) {
        returned = WaitForSingleObject(value.stop, 3000) == WAIT_OBJECT_0 &&
            SendMessageTimeoutW(value.window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result);
    }
    CloseHandle(value.stop); CloseHandle(value.process);
    // A failed/blocked restoration retains this single reference rather than leaving
    // callbacks into unloaded code. A successful UI round trip precedes unloading.
    if (returned) FreeLibraryAndExitThread(value.module, 0);
    return 1;
}
bool install(HWND panel, HWND owner) {
    for (unsigned i = 0; i < state.count; ++i) if (state.panels[i] == panel) return state.owner == owner;
    if (state.count == state.panels.size()) return false;
    if (state.count && (state.owner != owner || state.thread != GetCurrentThreadId())) return false;
    if (!state.count) {
        struct Threads { DWORD expected; bool compatible = true; } threads{GetCurrentThreadId()};
        EnumWindows([](HWND candidate, LPARAM parameter) -> BOOL {
            auto& result = *reinterpret_cast<Threads*>(parameter);
            DWORD pid{};
            const DWORD thread = GetWindowThreadProcessId(candidate, &pid);
            if (pid != GetCurrentProcessId()) return TRUE;
            wchar_t name[64]{};
            if (GetClassNameW(candidate, name, 64) &&
                (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd")) && thread != result.expected)
                result.compatible = false;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&threads));
        if (!threads.compatible) return false;
        DWORD pid{}; HANDLE process{};
        if (!ownerIsTrusted(owner, process, pid)) return false;
        const auto view = GetModuleHandleW(L"Taskbar.View.dll");
        constexpr GUID identity{0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}};
        if (!native_layout::matches(view, identity)) { CloseHandle(process); return false; }
        state.owner = owner; state.ownerPid = pid; state.primary = panel; state.process = process;
        state.thread = GetCurrentThreadId(); state.stopping = false;
        state.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        auto watcher = std::make_unique<Watch>();
        watcher->window = panel;
        bool prepared = state.stop &&
            DuplicateHandle(GetCurrentProcess(), process, GetCurrentProcess(), &watcher->process, 0, FALSE, DUPLICATE_SAME_ACCESS) &&
            DuplicateHandle(GetCurrentProcess(), state.stop, GetCurrentProcess(), &watcher->stop, 0, FALSE, DUPLICATE_SAME_ACCESS) &&
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCWSTR>(&watch), &watcher->module);
        if (!prepared || !SetWindowSubclass(panel, procedure, subclassId, 0) ||
            !state.patch.install(reinterpret_cast<BYTE*>(view) + 0x7bfa30, reinterpret_cast<const void*>(&showMenu))) {
            if (state.patch.active()) {
                // A protection/flush failure after writing code must never unload its
                // destination. Keep ownership and the recovery watcher until restored.
                state.panels[state.count++] = panel;
                state.stopping = true;
                SetPropW(panel, menu_protocol::readyProperty, owner);
                HANDLE recovery = CreateThread(nullptr, 0, watch, watcher.get(), 0, nullptr);
                if (recovery) { watcher.release(); CloseHandle(recovery); }
                else watcher.release(); // Retain this bounded safety reference on failure.
                report(E_FAIL);
                return false;
            }
            RemoveWindowSubclass(panel, procedure, subclassId);
            if (watcher->module) FreeLibrary(watcher->module);
            if (watcher->process) CloseHandle(watcher->process);
            if (watcher->stop) CloseHandle(watcher->stop);
            if (state.stop) CloseHandle(state.stop);
            CloseHandle(process); state.stop = nullptr; state.process = nullptr;
            return false;
        }
        state.panels[state.count++] = panel;
        HANDLE thread = CreateThread(nullptr, 0, watch, watcher.get(), 0, nullptr);
        if (!thread) {
            restore();
            CloseHandle(watcher->process); CloseHandle(watcher->stop); FreeLibrary(watcher->module);
            return false;
        }
        watcher.release(); CloseHandle(thread);
    } else {
        if (!SetWindowSubclass(panel, procedure, subclassId, 0)) return false;
        state.panels[state.count++] = panel;
    }
    return SetPropW(panel, menu_protocol::readyProperty, owner) != FALSE;
}
}
extern "C" __declspec(dllexport) LRESULT CALLBACK UmbrellaMenuHook(int code, WPARAM wp, LPARAM lp) {
    if (code >= 0) {
        const auto* call = reinterpret_cast<const CWPSTRUCT*>(lp);
        if (call && call->message == RegisterWindowMessageW(menu_protocol::attach) && call->wParam == menu_protocol::version) {
            wchar_t name[64]{};
            if (GetClassNameW(call->hwnd, name, 64) && (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd"))) {
                try { install(call->hwnd, reinterpret_cast<HWND>(call->lParam)); } catch (...) {}
            }
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}
