#include "TaskbarMenuProtocol.h"
#include "../SettingsWindowMessages.h"
#include <shellapi.h>
#include <array>
#include <cstdio>
#include <vector>
#include <string>

namespace {
using namespace umbrella::explorer;
struct Attachment { HWND panel{}; HHOOK hook{}; DWORD pid{}; };
std::vector<Attachment> attachments;
HWND owner{};
HMODULE module{};
HWINEVENTHOOK events{};
bool stopping = false;
unsigned attempts = 0;
constexpr UINT_PTR retryTimer = 1;
std::wstring executablePath() {
    std::array<wchar_t, 32768> path{};
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    return size && size < path.size() ? std::wstring(path.data(), size) : std::wstring{};
}
std::wstring sibling(const wchar_t* filename) {
    auto path = executablePath();
    const auto split = path.find_last_of(L'\\');
    return split != std::wstring::npos ? path.substr(0, split + 1) + filename : std::wstring{};
}
bool processPath(DWORD pid, const std::wstring& expected) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> path{};
    DWORD length = static_cast<DWORD>(path.size());
    const bool valid = QueryFullProcessImageNameW(process, 0, path.data(), &length) && !_wcsicmp(path.data(), expected.c_str());
    CloseHandle(process); return valid;
}
bool explorer(HWND panel) {
    wchar_t name[64]{};
    if (!GetClassNameW(panel, name, 64) || (wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd"))) return false;
    std::array<wchar_t, 32768> windows{};
    const auto length = GetWindowsDirectoryW(windows.data(), static_cast<UINT>(windows.size()));
    if (!length || length >= windows.size() || wcscat_s(windows.data(), windows.size(), L"\\explorer.exe")) return false;
    DWORD pid{}; GetWindowThreadProcessId(panel, &pid);
    return processPath(pid, windows.data());
}
bool request(HWND panel, const wchar_t* name, LPARAM parameter) {
    DWORD_PTR ignored{};
    return SendMessageTimeoutW(panel, RegisterWindowMessageW(name), menu_protocol::version, parameter,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &ignored) != 0;
}
void releaseHooks() {
    for (const auto& item : attachments) if (item.hook) UnhookWindowsHookEx(item.hook);
    attachments.clear();
}
bool attach(HWND panel) {
    DWORD pid{};
    const auto thread = GetWindowThreadProcessId(panel, &pid);
    for (auto iterator = attachments.begin(); iterator != attachments.end();) {
        DWORD current{};
        if (!IsWindow(iterator->panel) || !GetWindowThreadProcessId(iterator->panel, &current) || current != iterator->pid) {
            if (iterator->hook) UnhookWindowsHookEx(iterator->hook);
            iterator = attachments.erase(iterator);
        } else ++iterator;
    }
    for (const auto& item : attachments)
        if (item.panel == panel && GetPropW(panel, menu_protocol::readyProperty) == owner) return true;
    if (attachments.size() >= 16 || !thread) return false;
    auto entry = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "UmbrellaMenuHook"));
    if (!entry) return false;
    HHOOK hook = SetWindowsHookExW(WH_CALLWNDPROC, entry, module, thread);
    if (!hook) return false;
    if (!request(panel, menu_protocol::attach, reinterpret_cast<LPARAM>(owner)) ||
        GetPropW(panel, menu_protocol::readyProperty) != owner) {
        request(panel, menu_protocol::detach, 0);
        UnhookWindowsHookEx(hook); return false;
    }
    attachments.push_back({panel, hook, pid});
    return true;
}
BOOL CALLBACK collect(HWND panel, LPARAM parameter) {
    if (explorer(panel) && !attach(panel)) *reinterpret_cast<bool*>(parameter) = false;
    return TRUE;
}
void refresh() {
    KillTimer(owner, retryTimer);
    if (stopping) return;
    bool complete = true;
    EnumWindows(collect, reinterpret_cast<LPARAM>(&complete));
    if ((!complete || attachments.empty()) && ++attempts < 8) SetTimer(owner, retryTimer, 250, nullptr);
    std::fprintf(stderr, "Menu panels: %zu; compatible=%u\n", attachments.size(), complete ? 1u : 0u);
    std::fflush(stderr);
}
bool launch(const wchar_t* file, const wchar_t* parameters = nullptr) {
    SHELLEXECUTEINFOW operation{};
    operation.cbSize = sizeof(operation);
    operation.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    operation.hwnd = owner; operation.lpVerb = L"open"; operation.lpFile = file;
    operation.lpParameters = parameters; operation.nShow = SW_SHOWNORMAL;
    const bool result = ShellExecuteExW(&operation) != FALSE;
    if (operation.hProcess) CloseHandle(operation.hProcess);
    return result;
}
bool action(unsigned command) {
    if (command == 1 || command == 4) {
        const auto settings = sibling(L"UmbrellaShell.exe");
        HWND window = FindWindowW(nullptr, L"Umbrella — Настройки");
        if (window) {
            DWORD pid{}; GetWindowThreadProcessId(window, &pid);
            if (processPath(pid, settings)) {
                if (command == 4) {
                    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
                    if (!process) return false;
                    const bool requested = PostMessageW(window, RegisterWindowMessageW(umbrella::platform::settings_messages::quit), 0, 0) != FALSE;
                    const bool finished = requested && WaitForSingleObject(process, 5000) == WAIT_OBJECT_0;
                    CloseHandle(process); return finished;
                }
                AllowSetForegroundWindow(pid);
                return PostMessageW(window, RegisterWindowMessageW(umbrella::platform::settings_messages::show), 0, 0) != FALSE;
            }
        }
        return command == 4 || (!settings.empty() && launch(settings.c_str(), L"--settings"));
    }
    if (command == 2) {
        std::array<wchar_t, 32768> path{};
        const auto length = GetSystemDirectoryW(path.data(), static_cast<UINT>(path.size()));
        return length && length < path.size() && !wcscat_s(path.data(), path.size(), L"\\Taskmgr.exe") && launch(path.data());
    }
    return command == 3 && launch(L"ms-settings:taskbar");
}
void close() {
    stopping = true; KillTimer(owner, retryTimer);
    bool pending = false;
    for (const auto& item : attachments) {
        if (!IsWindow(item.panel)) continue;
        request(item.panel, menu_protocol::detach, 0);
        pending = pending || GetPropW(item.panel, menu_protocol::readyProperty) == owner;
    }
    if (!pending) { releaseHooks(); DestroyWindow(owner); }
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == menu_protocol::action && !stopping) {
        if (!action(static_cast<unsigned>(wp)))
            MessageBoxW(window, L"Не удалось открыть выбранный пункт. Проверьте установку Umbrella.", L"Umbrella", MB_OK | MB_ICONERROR);
        return 0;
    }
    if (message == menu_protocol::status) {
        std::fprintf(stderr, "Menu status: %08lx\n", static_cast<unsigned long>(lp)); std::fflush(stderr); return 0;
    }
    if (message == menu_protocol::detached) {
        if (stopping) close();
        else { releaseHooks(); attempts = 0; SetTimer(window, retryTimer, 250, nullptr); }
        return 0;
    }
    if (message == menu_protocol::refresh || message == RegisterWindowMessageW(L"TaskbarCreated")) {
        if (!stopping) { attempts = 0; SetTimer(window, retryTimer, 100, nullptr); } return 0;
    }
    if (message == WM_TIMER && wp == retryTimer) { refresh(); return 0; }
    if (message == WM_CLOSE || (message == WM_ENDSESSION && wp)) { close(); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wp, lp);
}
void CALLBACK shown(HWINEVENTHOOK, DWORD, HWND panel, LONG object, LONG child, DWORD, DWORD) {
    if (object == OBJID_WINDOW && child == CHILDID_SELF && explorer(panel))
        PostMessageW(owner, menu_protocol::refresh, 0, 0);
}
int run(int argc, wchar_t** argv) {
    const bool stop = argc == 2 && !wcscmp(argv[1], L"--stop");
    const bool status = argc == 2 && !wcscmp(argv[1], L"--status");
    const bool openSettings = argc == 2 && !wcscmp(argv[1], L"--open-settings");
    const bool quitSettings = argc == 2 && !wcscmp(argv[1], L"--quit-settings");
    if (argc > 2 || (argc == 2 && !stop && !status && !openSettings && !quitSettings && wcscmp(argv[1], L"--start"))) return 2;
    if (openSettings) return action(1) ? 0 : 1;
    if (quitSettings) return action(4) ? 0 : 1;
    HWND existing = FindWindowW(menu_protocol::ownerClass, nullptr);
    if (existing) {
        DWORD pid{}; GetWindowThreadProcessId(existing, &pid);
        if (!processPath(pid, executablePath())) return 3;
        if (!stop) return 0;
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!process) return 4;
        DWORD_PTR ignored{};
        const bool sent = SendMessageTimeoutW(existing, WM_CLOSE, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK, 4000, &ignored) != 0;
        const bool stopped = sent && WaitForSingleObject(process, 5000) == WAIT_OBJECT_0;
        CloseHandle(process); return stopped ? 0 : 5;
    }
    if (stop || status) return status ? 1 : 0;
    // Default DACL and Local namespace keep the single-instance guard in this logon session.
    HANDLE instance = CreateMutexW(nullptr, FALSE, L"Local\\Umbrella.TaskbarMenu.Instance.1");
    if (!instance) return 6;
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(instance); return 0; }
    const auto path = sibling(L"UmbrellaMenuModule.dll");
    module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) { CloseHandle(instance); return 7; }
    WNDCLASSW type{}; type.hInstance = GetModuleHandleW(nullptr); type.lpfnWndProc = procedure; type.lpszClassName = menu_protocol::ownerClass;
    if (!RegisterClassW(&type)) { FreeLibrary(module); CloseHandle(instance); return 8; }
    // An invisible top-level window receives Explorer's TaskbarCreated broadcast.
    owner = CreateWindowExW(WS_EX_TOOLWINDOW, type.lpszClassName, L"Umbrella taskbar menu", WS_POPUP, 0,0,0,0,
        nullptr, nullptr, type.hInstance, nullptr);
    if (!owner) { FreeLibrary(module); CloseHandle(instance); return 9; }
    events = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, shown, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    refresh();
    MSG message{};
    int result{};
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    if (events) UnhookWinEvent(events);
    releaseHooks(); FreeLibrary(module); CloseHandle(instance);
    return result < 0 ? 10 : 0;
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc{};
    auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    const int result = run(argc, argv); LocalFree(argv); return result;
}
