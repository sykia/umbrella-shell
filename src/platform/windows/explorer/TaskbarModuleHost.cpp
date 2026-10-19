#include "TaskbarModuleProtocol.h"
#include "NativeTaskbarLayout.h"
#include "TaskbarIslands.h"
#include "TaskbarPopupPlacement.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using namespace umbrella::explorer;
struct Attachment { HWND window{}; HHOOK hook{}; RECT before{}; };
std::vector<Attachment> attachments;
HWND owner{};
HMODULE module{};
bool failed = false;
HANDLE inputThread{};
unsigned pendingWidth{}, pendingEdge{}, pendingThickness{};
bool maskPending = false, extendedAck = false, pendingDetached = false;
constexpr UINT configureMessage = WM_APP + 19;
HWINEVENTHOOK popupHook{};
bool layoutActive = false;
unsigned currentEdge = 3;
void CALLBACK popupChanged(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD);
bool isExplorer(HWND window) {
    wchar_t name[64]{};
    if (!GetClassNameW(window, name, 64) || (wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd"))) return false;
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> path{}, expected{};
    DWORD size = static_cast<DWORD>(path.size());
    const auto length = GetWindowsDirectoryW(expected.data(), static_cast<UINT>(expected.size()));
    const bool ok = length && length < expected.size() &&
        wcscat_s(expected.data(), expected.size(), L"\\explorer.exe") == 0 &&
        QueryFullProcessImageNameW(process, 0, path.data(), &size) && !_wcsicmp(path.data(), expected.data());
    CloseHandle(process);
    return ok;
}
bool send(HWND target, UINT message, WPARAM wp, LPARAM lp, DWORD_PTR& result) {
    return SendMessageTimeoutW(target, message, wp, lp, SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result) != 0;
}
bool configure(HWND window, unsigned width, bool enabled, unsigned edge = 3, unsigned thickness = 48, bool islandsMode = false, bool detached = false) {
    Configuration config;
    config.widthPercent = width;
    config.enabled = enabled;
    config.edge = edge;
    config.thickness = thickness;
    config.islands = islandsMode;
    config.detached = detached || islandsMode;
    COPYDATASTRUCT packet{protocol, sizeof(config), &config};
    DWORD_PTR result{};
    return send(window, WM_COPYDATA, reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(&packet), result) && result;
}
bool attach(HWND target) {
    if (attachments.size() == 16) return false;
    const auto thread = GetWindowThreadProcessId(target, nullptr);
    const auto entry = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "UmbrellaTaskbarHook"));
    Attachment value{target};
    if (!thread || !entry || !GetWindowRect(target, &value.before)) return false;
    value.hook = SetWindowsHookExW(WH_CALLWNDPROC, entry, module, thread);
    if (!value.hook) return false;
    attachments.push_back(value); // Also clean up an attachment if handshake fails halfway.
    DWORD_PTR result{};
    return send(target, RegisterWindowMessageW(attachMessage), protocol, reinterpret_cast<LPARAM>(owner), result)
        && configure(target, 100, false);
}
void restore() {
    layoutActive = false;
    if (popupHook) UnhookWinEvent(popupHook); popupHook = nullptr;
    islands::stop();
    for (const auto& value : attachments) {
        DWORD_PTR result{};
        if (!send(value.window, RegisterWindowMessageW(restoreMessage), 0, 0, result)) failed = true;
        if (value.hook && !UnhookWindowsHookEx(value.hook)) failed = true;
        RECT now{};
        if (GetWindowRect(value.window, &now) &&
            (now.left != value.before.left || now.right != value.before.right)) failed = true;
    }
    attachments.clear();
}
LRESULT CALLBACK ownerProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == RegisterWindowMessageW(L"Umbrella.Taskbar.MenuOpened.1")) {
        std::fprintf(stderr, "Taskbar menu status: %08lx\n", static_cast<unsigned long>(lp));
        std::fflush(stderr);
        return 0;
    }
    if (message == configureMessage) {
        const auto width = static_cast<unsigned>(wp);
        const auto packed = static_cast<unsigned>(lp);
        const auto edge = packed & 3u;
        const auto thickness = (packed >> 2) & 127u;
        const bool islandsMode = (packed & (1u << 9)) != 0;
        const bool detached = (packed & (1u << 10)) != 0;
        extendedAck = (packed & (1u << 11)) != 0;
        pendingDetached = detached;
        bool ok = width >= 40 && width <= 100 && edge <= 3 && thickness >= 48 && thickness <= 96;
        if (ok) for (const auto& value : attachments) {
            DWORD_PTR suspended{};
            ok = send(value.window, RegisterWindowMessageW(suspendVisualsMessage), protocol, reinterpret_cast<LPARAM>(owner), suspended) && suspended && ok;
        }
        if (ok) for (const auto& value : attachments) ok = configure(value.window, width, true, edge, thickness, islandsMode, detached) && ok;
        if (ok) {
            layoutActive = true; currentEdge = edge;
            if (!popupHook && !attachments.empty()) {
                DWORD pid{}; GetWindowThreadProcessId(attachments.front().window, &pid);
                popupHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_LOCATIONCHANGE, nullptr, popupChanged, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
                if (!popupHook) ok = false;
            }
        }
        ok = islands::setEnabled(ok && islandsMode) && ok;
        if (ok && islandsMode) {
            pendingWidth = width; pendingEdge = edge; pendingThickness = thickness; maskPending = true;
            SetTimer(window, 2, 2500, nullptr);
            return 0;
        }
        maskPending = false;
        KillTimer(window, 2);
        std::printf(ok ? "applied %u %u %u %u" : "error %u %u %u %u", width, edge, thickness, islandsMode ? 1u : 0u);
        if (extendedAck) std::printf(" %u", detached ? 1u : 0u);
        std::puts("");
        std::fflush(stdout);
        return 0;
    }
    if (message == islands::maskReady || (message == WM_TIMER && wp == 2)) {
        if (maskPending) {
            KillTimer(window, 2); maskPending = false;
            std::printf(message == islands::maskReady && wp ? "applied %u %u %u 1" : "error %u %u %u 1",
                pendingWidth, pendingEdge, pendingThickness);
            if (extendedAck) std::printf(" %u", pendingDetached ? 1u : 0u);
            std::puts("");
            std::fflush(stdout);
        }
        return 0;
    }
    if (message == WM_TIMER || message == WM_CLOSE) {
        KillTimer(window, 1);
        restore();
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wp, lp);
}
void CALLBACK popupChanged(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD) {
    if (!layoutActive || object != OBJID_WINDOW || child != CHILDID_SELF ||
        (event != EVENT_OBJECT_SHOW && event != EVENT_OBJECT_LOCATIONCHANGE) ||
        !popup_placement::overflow(window)) return;
    RECT rectangle{};
    if (!GetWindowRect(window, &rectangle) || rectangle.right <= rectangle.left || rectangle.bottom <= rectangle.top) return;
    const auto monitor = MonitorFromRect(&rectangle, MONITOR_DEFAULTTONEAREST);
    for (const auto& value : attachments) {
        if (MonitorFromRect(&value.before, MONITOR_DEFAULTTONEAREST) != monitor) continue;
        RECT panel{}; MONITORINFO info{sizeof(info)};
        if (!GetWindowRect(value.window, &panel) || !GetMonitorInfoW(monitor, &info)) return;
        const auto point = popup_placement::position(panel, rectangle, info.rcMonitor, currentEdge,
            MulDiv(8, static_cast<int>(GetDpiForWindow(value.window)), 96));
        if (point.x != rectangle.left || point.y != rectangle.top)
            SetWindowPos(window, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        return;
    }
}
BOOL CALLBACK collect(HWND window, LPARAM) {
    if (isExplorer(window) && !attach(window)) failed = true;
    return attachments.size() < 16;
}
}

int wmain(int argc, wchar_t** argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (argc == 2 && !wcscmp(argv[1], L"--capabilities")) {
        std::puts(native_layout::installedBuildSupported() ? "native-layout" : "width-only");
        return 0;
    }
    const bool fixture = argc == 2 && !wcscmp(argv[1], L"--fixture");
    const bool serve = argc == 2 && !wcscmp(argv[1], L"--serve");
    if (serve && FindWindowExW(HWND_MESSAGE, nullptr, ownerClass, nullptr)) {
        std::puts("FAILED; another Umbrella taskbar session is active"); return 5;
    }
    if (!fixture && !serve && (argc != 5 || wcscmp(argv[1], L"--width") || wcscmp(argv[3], L"--seconds"))) {
        std::puts("Usage: UmbrellaTaskbarHost --fixture | --serve | --width 40..100 --seconds 1..120");
        return 2;
    }
    unsigned width = fixture ? 75 : serve ? 100 : static_cast<unsigned>(_wtoi(argv[2]));
    unsigned seconds = fixture || serve ? 1 : static_cast<unsigned>(_wtoi(argv[4]));
    if (width < 40 || width > 100 || seconds < 1 || seconds > 120) return 2;
    std::array<wchar_t, 32768> path{};
    GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    auto* end = wcsrchr(path.data(), L'\\');
    if (!end) return 3;
    *(end + 1) = 0;
    if (wcscat_s(path.data(), path.size(), L"UmbrellaTaskbarModule.dll")) return 3;
    module = LoadLibraryExW(path.data(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) { std::printf("Load module failed: %lu\n", GetLastError()); return 3; }
    WNDCLASSW ownerType{};
    ownerType.lpfnWndProc = ownerProcedure;
    ownerType.hInstance = GetModuleHandleW(nullptr);
    ownerType.lpszClassName = ownerClass;
    if (!RegisterClassW(&ownerType)) return 4;
    owner = CreateWindowW(ownerClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, ownerType.hInstance, nullptr);
    HWND test{};
    if (fixture) {
        WNDCLASSW testType = ownerType;
        testType.lpszClassName = L"Umbrella.Taskbar.ModuleFixture.1";
        testType.lpfnWndProc = DefWindowProcW;
        RegisterClassW(&testType);
        test = CreateWindowW(testType.lpszClassName, L"Umbrella module geometry test", WS_POPUP,
                             0, 0, GetSystemMetrics(SM_CXSCREEN), 48, nullptr, nullptr, testType.hInstance, nullptr);
        failed = !test || !attach(test);
    } else EnumWindows(collect, 0);
    if (attachments.empty()) failed = true;
    if (!serve) for (const auto& value : attachments) {
        if (!configure(value.window, width, true)) { failed = true; break; }
        RECT now{};
        GetWindowRect(value.window, &now);
        std::printf("Applied: x=%ld y=%ld width=%ld height=%ld\n", now.left, now.top, now.right - now.left, now.bottom - now.top);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(value.window, MONITOR_DEFAULTTONEAREST), &monitor);
        const int wanted = MulDiv(monitor.rcMonitor.right - monitor.rcMonitor.left, static_cast<int>(width), 100);
        if (now.right - now.left != wanted) failed = true;
        if (fixture) {
            if (configure(value.window, 39, true) || configure(value.window, 101, true)) failed = true;
            SetWindowPos(value.window, nullptr, 0, 0, GetSystemMetrics(SM_CXSCREEN), 48, SWP_NOZORDER | SWP_NOACTIVATE);
            GetWindowRect(value.window, &now);
            if (now.right - now.left != wanted) failed = true;
            if (configure(value.window, width, true, 3, 47) || configure(value.window, width, true, 3, 97) ||
                configure(value.window, width, true, 3, 48, true)) failed = true;
            if (!configure(value.window, width, true, 3, 72, true)) failed = true;
            GetWindowRect(value.window, &now);
            if (now.bottom - now.top != 72) failed = true;
            IslandMask mask; mask.bounds = now; mask.count = 3;
            mask.rectangles[0] = {10, 6, 130, 66};
            mask.rectangles[1] = {150, 6, 450, 66};
            mask.rectangles[2] = {470, 6, 590, 66};
            COPYDATASTRUCT packet{islandsProtocol, sizeof(mask), &mask};
            DWORD_PTR accepted{};
            if (!send(value.window, WM_COPYDATA, reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(&packet), accepted) || !accepted) failed = true;
            HRGN region = CreateRectRgn(0, 0, 0, 0);
            if (!region || GetWindowRgn(value.window, region) == ERROR || !PtInRegion(region, 50, 30) ||
                PtInRegion(region, 140, 30) || !PtInRegion(region, 250, 30)) failed = true;
            if (region) DeleteObject(region);
            mask.count = 0;
            if (!send(value.window, WM_COPYDATA, reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(&packet), accepted) || accepted) failed = true;
            mask.count = 3; mask.bounds.left++;
            if (!send(value.window, WM_COPYDATA, reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(&packet), accepted) || accepted) failed = true;
            // Exercise the actual overflow subclass without opening the system tray.
            WNDCLASSW popupType{}; popupType.lpfnWndProc = DefWindowProcW;
            popupType.hInstance = GetModuleHandleW(nullptr); popupType.lpszClassName = L"TopLevelWindowForOverflowXamlIsland";
            RegisterClassW(&popupType);
            if (!configure(value.window, width, true, 1, 72, false, true)) failed = true;
            HWND popup = CreateWindowW(popupType.lpszClassName, L"", WS_POPUP, 100, 1400, 260, 200,
                value.window, nullptr, popupType.hInstance, nullptr);
            SetWindowPos(popup, nullptr, 100, 1400, 260, 200, SWP_NOZORDER | SWP_NOACTIVATE);
            RECT panel{}, flyout{};
            GetWindowRect(value.window, &panel); GetWindowRect(popup, &flyout);
            if (!popup || GetPropW(popup, popup_placement::ownerProperty) != owner || flyout.top != panel.bottom + 8) failed = true;
            SetWindowPos(popup, nullptr, 3000, -600, 260, 200, SWP_NOZORDER | SWP_NOACTIVATE);
            GetWindowRect(popup, &flyout);
            if (flyout.right > monitor.rcMonitor.right || flyout.left < monitor.rcMonitor.left || flyout.top != panel.bottom + 8) failed = true;
            if (!configure(value.window, width, true, 3, 72, false, true)) failed = true;
            SetWindowPos(popup, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            GetWindowRect(value.window, &panel); GetWindowRect(popup, &flyout);
            if (flyout.bottom != panel.top - 8) failed = true;
            if (popup) DestroyWindow(popup);
        }
    }
    std::fflush(stdout);
    if (serve && !failed) {
        std::vector<HWND> windows;
        for (const auto& value : attachments) windows.push_back(value.window);
        if (!islands::start(owner, windows)) { restore(); return 5; }
        inputThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
            char line[32]{};
            while (std::fgets(line, sizeof(line), stdin)) {
                unsigned width{}, edge{}, thickness = 48, mode = 0, detached = 0; char extra{};
                const int parsed = sscanf_s(line, "%u %u %u %u %u %c", &width, &edge, &thickness, &mode, &detached, &extra, 1u);
                if ((parsed == 2 || parsed == 4 || parsed == 5) && width >= 40 && width <= 100 && edge <= 3 && thickness >= 48 && thickness <= 96 && mode <= 1 && detached <= 1)
                    PostMessageW(owner, configureMessage, static_cast<WPARAM>(width), static_cast<LPARAM>(edge | (thickness << 2) | (mode << 9) | (detached << 10) | (parsed == 5 ? 1u << 11 : 0u)));
                else PostMessageW(owner, WM_CLOSE, 0, 0);
            }
            PostMessageW(owner, WM_CLOSE, 0, 0);
            return 0;
        }, nullptr, 0, nullptr);
        if (!inputThread) { failed = true; PostMessageW(owner, WM_CLOSE, 0, 0); }
        else { std::puts("ready"); std::fflush(stdout); }
    } else if (failed || fixture) {
        restore();
        if (test) DestroyWindow(test);
        DestroyWindow(owner);
    } else SetTimer(owner, 1, seconds * 1000, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (inputThread) {
        CancelSynchronousIo(inputThread);
        WaitForSingleObject(inputThread, 3000);
        CloseHandle(inputThread);
    }
    FreeLibrary(module);
    std::puts(failed ? "FAILED; restore requested" : "Passed; original geometry restored");
    return failed ? 1 : 0;
}
