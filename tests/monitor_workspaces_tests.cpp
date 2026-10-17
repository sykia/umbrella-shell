#include "platform/windows/MonitorWorkspaces.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>
using namespace umbrella::platform;
namespace {
struct Close { void operator()(void* value) const { if (value) CloseHandle(value); } };
using Handle = std::unique_ptr<void, Close>;
struct Child { HANDLE value; ~Child() { if (value) { TerminateProcess(value, 0); WaitForSingleObject(value, 3000); } } };
HWND childWindow = nullptr;
HWND framelessWindow = nullptr, toolWindow = nullptr;
HWND createWindow();
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_APP + 1) { const HWND added = createWindow(); ShowWindow(added, SW_SHOWNOACTIVATE); return reinterpret_cast<LRESULT>(added); }
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
        const HWND frameless = CreateWindowExW(WS_EX_APPWINDOW, L"UmbrellaWorkspaceFixture", L"Frameless workspace fixture",
            WS_POPUP | WS_THICKFRAME, 120, 120, 240, 160, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        const HWND tool = CreateWindowExW(WS_EX_TOOLWINDOW, L"UmbrellaWorkspaceFixture", L"Workspace tool fixture",
            WS_OVERLAPPEDWINDOW, 140, 140, 240, 160, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!frameless || !tool) return 1;
        ShowWindow(hwnd, SW_SHOWNORMAL);
        ShowWindow(frameless, SW_SHOWNORMAL); ShowWindow(tool, SW_SHOWNOACTIVATE);
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
    EnumWindows([](HWND hwnd, LPARAM pid) -> BOOL {
        DWORD owner = 0; GetWindowThreadProcessId(hwnd, &owner);
        if (owner == static_cast<DWORD>(pid) && IsWindowVisible(hwnd)) {
            const auto extended = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if (extended & WS_EX_TOOLWINDOW) toolWindow = hwnd;
            else if (extended & WS_EX_APPWINDOW) framelessWindow = hwnd;
            else childWindow = hwnd;
        }
        return TRUE;
    }, process.dwProcessId);
    const HWND panel = createWindow(); if (!panel || !childWindow || !framelessWindow || !toolWindow) return 1;
    SetPropW(panel, L"UmbrellaShell.AppBar", reinterpret_cast<HANDLE>(1));
    WorkspaceSnapshot owner, guardian;
    if (!owner.open(name, true) || !guardian.open(name, false)) return 1;
    // Restrict discovery to this fixture process: no real desktop windows can be hidden.
    MonitorWorkspaces manager(nullptr, process.dwProcessId);
    if (!manager.start(QString::fromStdWString(name))) return 1;
    const auto model = manager.model(reinterpret_cast<quintptr>(panel));
    if (model.size() != 5 || !model[0].toMap()["active"].toBool() || !model[0].toMap()["occupied"].toBool()) return 1;
    if (manager.select(reinterpret_cast<quintptr>(panel), -1) || manager.select(reinterpret_cast<quintptr>(panel), 5)) return 1;
    if (!manager.select(reinterpret_cast<quintptr>(panel), 1) || IsWindowVisible(childWindow) || IsWindowVisible(framelessWindow) || !IsWindowVisible(toolWindow)) {
        std::cerr << "Switch must hide conventional and frameless app windows, excluding tool windows\n"; return 1;
    }
    if (!manager.select(reinterpret_cast<quintptr>(panel), 0) || !IsWindowVisible(childWindow) || !IsWindowVisible(framelessWindow)) return 1;
    QElapsedTimer elapsed; elapsed.start(); qint64 longestSwitch = 0;
    for (int i = 0; i < 50; ++i) {
        const auto before = elapsed.nsecsElapsed();
        if (!manager.select(reinterpret_cast<quintptr>(panel), (i + 1) % 2)) return 1;
        longestSwitch = (std::max)(longestSwitch, elapsed.nsecsElapsed() - before);
        const bool visible = i % 2 != 0;
        if ((IsWindowVisible(childWindow) != FALSE) != visible || (IsWindowVisible(framelessWindow) != FALSE) != visible) return 1;
    }
    std::cout << "50 switches: totalMs=" << elapsed.elapsed() << " longestMs=" << longestSwitch / 1000000.0 << '\n';
    // A switch must not restore a minimized window or unmaximize a maximized one.
    ShowWindow(childWindow, SW_SHOWMAXIMIZED); ShowWindow(framelessWindow, SW_MINIMIZE);
    if (!manager.select(reinterpret_cast<quintptr>(panel), 1) || !manager.select(reinterpret_cast<quintptr>(panel), 0) ||
        !IsZoomed(childWindow) || !IsIconic(framelessWindow)) { std::cerr << "Window placement changed during switch\n"; return 1; }
    ShowWindow(childWindow, SW_RESTORE); ShowWindow(framelessWindow, SW_RESTORE);
    if (!manager.select(reinterpret_cast<quintptr>(panel), 1)) return 1;
    DWORD_PTR addedValue = 0;
    if (!SendMessageTimeoutW(childWindow, WM_APP + 1, 0, 0, SMTO_ABORTIFHUNG, 2000, &addedValue) || !addedValue) return 1;
    const auto added = reinterpret_cast<HWND>(addedValue);
    manager.refresh();
    if (!manager.model(reinterpret_cast<quintptr>(panel))[1].toMap()["occupied"].toBool() ||
        !manager.select(reinterpret_cast<quintptr>(panel), 0) || IsWindowVisible(added) ||
        !manager.select(reinterpret_cast<quintptr>(panel), 1) || !IsWindowVisible(added)) {
        std::cerr << "New window was not assigned to the active group\n"; return 1;
    }
    if (!guardian.restore() || !IsWindowVisible(childWindow)) return 1;
    if (!manager.stop() || !IsWindowVisible(childWindow)) return 1;
    if (!manager.start(QString::fromStdWString(name)) || !manager.select(reinterpret_cast<quintptr>(panel), 4) || IsWindowVisible(childWindow)) return 1;
    // Invalid counts must not alter visibility; shrinking must recover removed groups.
    if (manager.setCount(0) || manager.setCount(11) || IsWindowVisible(childWindow)) return 1;
    if (!manager.setCount(2) || !IsWindowVisible(childWindow)) return 1;
    const auto resized = manager.model(reinterpret_cast<quintptr>(panel));
    if (resized.size() != 2 || !resized[0].toMap()["active"].toBool() || !resized[0].toMap()["occupied"].toBool()) return 1;
    if (manager.select(reinterpret_cast<quintptr>(panel), 2) || !manager.stop()) return 1;
    std::vector<RECT> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT rect, LPARAM data) -> BOOL {
        reinterpret_cast<std::vector<RECT>*>(data)->push_back(*rect); return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    if (monitors.size() > 1) {
        const auto& first = monitors[0]; const auto& second = monitors[1];
        const HWND otherPanel = createWindow(); if (!otherPanel) return 1;
        SetPropW(otherPanel, L"UmbrellaShell.AppBar", reinterpret_cast<HANDLE>(1));
        SetWindowPos(panel, nullptr, first.left + 100, first.top + 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(otherPanel, nullptr, second.left + 100, second.top + 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(childWindow, nullptr, first.left + 100, first.top + 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(framelessWindow, nullptr, second.left + 100, second.top + 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        if (!manager.start(QString::fromStdWString(name)) || !manager.select(reinterpret_cast<quintptr>(panel), 1) ||
            IsWindowVisible(childWindow) || !IsWindowVisible(framelessWindow) ||
            !manager.model(reinterpret_cast<quintptr>(otherPanel))[0].toMap()["active"].toBool()) {
            std::cerr << "Switch affected another monitor\n"; return 1;
        }
        if (!manager.select(reinterpret_cast<quintptr>(otherPanel), 1) || IsWindowVisible(framelessWindow) ||
            !manager.select(reinterpret_cast<quintptr>(panel), 0) || !IsWindowVisible(childWindow) || IsWindowVisible(framelessWindow) || !manager.stop()) return 1;
        RemovePropW(otherPanel, L"UmbrellaShell.AppBar"); DestroyWindow(otherPanel);
        std::cout << "Independent switching on two monitors passed\n";
    } else std::cout << "Two-monitor check skipped: only one monitor available\n";
    RemovePropW(panel, L"UmbrellaShell.AppBar"); DestroyWindow(panel);
    std::cout << "Per-monitor workspace model, invalid slots, hide and guardian restore passed\n";
    return 0;
}
