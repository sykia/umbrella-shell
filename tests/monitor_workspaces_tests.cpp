#include "platform/windows/MonitorWorkspaces.h"
#include <QCoreApplication>
#include <windows.h>
#include <iostream>
#include <memory>
using namespace umbrella::platform;
namespace {
struct Close { void operator()(void* value) const { if (value) CloseHandle(value); } };
using Handle = std::unique_ptr<void, Close>;
struct Child { HANDLE value; ~Child() { if (value) { TerminateProcess(value, 0); WaitForSingleObject(value, 3000); } } };
HWND childWindow = nullptr;
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, message, w, l);
}
HWND createWindow() {
    WNDCLASSW type{}; type.lpfnWndProc = windowProc; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = L"UmbrellaWorkspaceFixture";
    RegisterClassW(&type);
    return CreateWindowExW(0, type.lpszClassName, L"Workspace test fixture", WS_OVERLAPPEDWINDOW, 100, 100, 240, 160, nullptr, nullptr, type.hInstance, nullptr);
}
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == "--child") {
        const HWND hwnd = createWindow(); if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOWNORMAL);
        Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, arguments[2].toStdWString().c_str()));
        if (!ready || !SetEvent(ready.get())) return 1;
        MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        return 0;
    }
    const auto name = L"Local\\Umbrella.WorkspaceTest." + std::to_wstring(GetCurrentProcessId());
    Handle ready(CreateEventW(nullptr, TRUE, FALSE, (name + L".ready").c_str()));
    wchar_t path[32768]{}; if (!ready || !GetModuleFileNameW(nullptr, path, 32768)) return 1;
    std::wstring command = L"\"" + std::wstring(path) + L"\" --child \"" + name + L".ready\"";
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if (!CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return 1;
    Handle childHandle(process.hProcess), thread(process.hThread); Child cleanup{process.hProcess};
    if (WaitForSingleObject(ready.get(), 5000) != WAIT_OBJECT_0) return 1;
    EnumWindows([](HWND hwnd, LPARAM pid) -> BOOL { DWORD owner = 0; GetWindowThreadProcessId(hwnd, &owner); if (owner == static_cast<DWORD>(pid) && IsWindowVisible(hwnd)) { childWindow = hwnd; return FALSE; } return TRUE; }, process.dwProcessId);
    const HWND panel = createWindow(); if (!panel || !childWindow) return 1;
    SetPropW(panel, L"UmbrellaShell.AppBar", reinterpret_cast<HANDLE>(1));
    WorkspaceSnapshot owner, guardian;
    if (!owner.open(name, true) || !guardian.open(name, false)) return 1;
    // Restrict discovery to this fixture process: no real desktop windows can be hidden.
    MonitorWorkspaces manager(nullptr, process.dwProcessId);
    if (!manager.start(QString::fromStdWString(name))) return 1;
    const auto model = manager.model(reinterpret_cast<quintptr>(panel));
    if (model.size() != 5 || !model[0].toMap()["active"].toBool() || !model[0].toMap()["occupied"].toBool()) return 1;
    if (manager.select(reinterpret_cast<quintptr>(panel), -1) || manager.select(reinterpret_cast<quintptr>(panel), 5)) return 1;
    if (!manager.select(reinterpret_cast<quintptr>(panel), 1) || IsWindowVisible(childWindow)) return 1;
    if (!guardian.restore() || !IsWindowVisible(childWindow)) return 1;
    if (!manager.stop() || !IsWindowVisible(childWindow)) return 1;
    if (!manager.start(QString::fromStdWString(name)) || !manager.select(reinterpret_cast<quintptr>(panel), 4) || IsWindowVisible(childWindow)) return 1;
    // Invalid counts must not alter visibility; shrinking must recover removed groups.
    if (manager.setCount(0) || manager.setCount(11) || IsWindowVisible(childWindow)) return 1;
    if (!manager.setCount(2) || !IsWindowVisible(childWindow)) return 1;
    const auto resized = manager.model(reinterpret_cast<quintptr>(panel));
    if (resized.size() != 2 || !resized[0].toMap()["active"].toBool() || !resized[0].toMap()["occupied"].toBool()) return 1;
    if (manager.select(reinterpret_cast<quintptr>(panel), 2) || !manager.stop()) return 1;
    RemovePropW(panel, L"UmbrellaShell.AppBar"); DestroyWindow(panel);
    std::cout << "Per-monitor workspace model, invalid slots, hide and guardian restore passed\n";
    return 0;
}
