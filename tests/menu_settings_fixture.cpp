#include <Windows.h>
#include <shellapi.h>
#include <array>
#include <cwchar>
#include <cstring>
#include "platform/windows/SettingsWindowMessages.h"
bool marker(const wchar_t* filename, const char* text) {
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return false;
    auto* end = wcsrchr(path.data(), L'\\');
    if (!end) return false;
    *(end + 1) = 0;
    if (wcscat_s(path.data(), path.size(), filename)) return false;
    HANDLE file = CreateFileW(path.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written{};
    const auto size = static_cast<DWORD>(strlen(text));
    const bool saved = WriteFile(file, text, size, &written, nullptr) && written == size;
    CloseHandle(file); return saved;
}
LRESULT CALLBACK fixture(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    using namespace umbrella::platform;
    if (message == RegisterWindowMessageW(settings_messages::show)) {
        marker(L"settings-reopened.txt", "reopened"); return 0;
    }
    if (message == RegisterWindowMessageW(settings_messages::quit)) {
        marker(L"settings-quit.txt", "quit"); DestroyWindow(window); return 0;
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wp, lp);
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc{};
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const bool valid = argv && argc == 2 && !wcscmp(argv[1], L"--settings");
    const bool windowFixture = argv && argc == 2 && !wcscmp(argv[1], L"--window-fixture");
    if (argv) LocalFree(argv);
    if (valid) return marker(L"settings-invoked.txt", "--settings") ? 0 : 7;
    if (!windowFixture) return 2;
    WNDCLASSW type{}; type.hInstance = GetModuleHandleW(nullptr); type.lpfnWndProc = fixture; type.lpszClassName = L"Umbrella.Settings.TestFixture";
    if (!RegisterClassW(&type)) return 8;
    HWND window = CreateWindowW(type.lpszClassName, L"Umbrella — Настройки", WS_POPUP, 0,0,0,0,
        nullptr,nullptr,type.hInstance,nullptr);
    if (!window || !marker(L"settings-ready.txt", "ready")) return 9;
    // No window is shown and no taskbar setting is changed.
    MSG message{};
    while (GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return 0;
}
