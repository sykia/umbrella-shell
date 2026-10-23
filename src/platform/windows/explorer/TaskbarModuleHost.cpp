#include "TaskbarModuleProtocol.h"
#include "TaskbarPlacementCommand.h"
#include "../TaskbarWindows.h"
#include "NativeTaskbarLayout.h"
#include "TaskbarIslands.h"
#include "TaskbarPopupPlacement.h"
#include "TaskbarButtonEditVerification.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <shellapi.h>
#include <vector>
#include <mutex>

namespace {
using namespace umbrella::explorer;
struct Attachment { HWND window{}; HHOOK hook{}; RECT before{}; bool islands = false; unsigned edge = 3; bool readOnly = false; };
std::vector<Attachment> attachments;
HWND owner{};
HMODULE module{};
bool failed = false;
HANDLE inputThread{};
unsigned pendingWidth{}, pendingEdge{}, pendingThickness{};
bool maskPending = false, extendedAck = false, pendingDetached = false, pendingIslands = false;
constexpr UINT configureMessage = WM_APP + 19;
HWINEVENTHOOK popupHook{};
bool layoutActive = false;
bool editExperiment = false, reportReceived = false;
bool verifyOwnerDeath = false;
bool configureTaskview = false, verifyPlacement = false;
bool inspectComponents = false;
bool inspectFixture = false;
bool placementSession=false;
constexpr UINT placementMessage=WM_APP+23;
std::mutex placementMutex;
PlacementCommand pendingPlacement;
ComponentLayoutReport placementReply;
bool placementReceived=false;
bool fixtureNativeTouched = false;
LRESULT CALLBACK observerFixtureProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if (message==WM_COPYDATA || message==RegisterWindowMessageW(attachMessage) || message==RegisterWindowMessageW(restoreMessage)) fixtureNativeTouched=true;
    return DefWindowProcW(window,message,wp,lp);
}
HWND snapshotWindow{};
HWND nativeEditTarget{};
EditReport editReport;
std::mutex editReportMutex;
constexpr UINT editMessage = WM_APP + 20;
unsigned currentEdge = 3;
constexpr UINT monitorMessage = WM_APP + 21;
constexpr UINT trayMessage=WM_APP+22;
std::mutex trayRequestMutex;
char pendingTrayDevice[64]{};
struct MonitorRequest { unsigned width{}, packed{}; char device[64]{}; };
char pendingMonitor[64]{};
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
    const auto entry = reinterpret_cast<HOOKPROC>(GetProcAddress(module, inspectComponents ? "UmbrellaTaskbarInspectHook" : "UmbrellaTaskbarHook"));
    Attachment value{target};
    if (!thread || !entry || !GetWindowRect(target, &value.before)) return false;
    value.readOnly=inspectComponents;
    const bool existingObserver=inspectComponents && std::any_of(attachments.begin(),attachments.end(),[&](const auto& item){return item.readOnly && GetWindowThreadProcessId(item.window,nullptr)==thread;});
    if (!existingObserver) {
        value.hook = SetWindowsHookExW(WH_CALLWNDPROC, entry, module, thread);
        if (!value.hook) return false;
    }
    attachments.push_back(value); // Also clean up an attachment if handshake fails halfway.
    if (inspectComponents) return true; // Observation never performs an attach handshake.
    DWORD_PTR result{};
    return send(target, RegisterWindowMessageW(attachMessage), protocol, reinterpret_cast<LPARAM>(owner), result)
        && configure(target, 100, false);
}
void restore() {
    if (editExperiment && !inspectComponents && attachments.size() == 1 && !SendMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::Reset), 0)) failed = true;
    layoutActive = false;
    if (popupHook) UnhookWinEvent(popupHook); popupHook = nullptr;
    islands::stop();
    for (const auto& value : attachments) {
        DWORD_PTR result{};
        if (!value.readOnly && (!send(value.window, RegisterWindowMessageW(restoreMessage), 0, 0, result) || !result)) failed = true;
        if (value.hook && !UnhookWindowsHookEx(value.hook)) failed = true;
        RECT now{};
        if (GetWindowRect(value.window, &now) &&
            (now.left != value.before.left || now.right != value.before.right ||
             (value.readOnly && (now.top!=value.before.top || now.bottom!=value.before.bottom)))) failed = true;
    }
    attachments.clear();
}
LRESULT CALLBACK ownerProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if(message==WM_COPYDATA && nativeEditTarget && reinterpret_cast<HWND>(wp)==nativeEditTarget) {
        const auto* packet=reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if(packet && packet->dwData==placementReportProtocol) {
            if(packet->cbData!=sizeof(ComponentLayoutReport) || !packet->lpData)return FALSE;
            const auto reply=*static_cast<const ComponentLayoutReport*>(packet->lpData);
            if(reply.size!=sizeof(reply) || reply.version!=1 || reply.bound>1 || reply.items>64 || reply.failurePhase>7)return FALSE;
            placementReply=reply;placementReceived=true;return TRUE;
        }
    }
    if(message==placementMessage) {
        PlacementCommand command;
        {const std::lock_guard lock(placementMutex);command=pendingPlacement;}
        for(const auto& attachment:attachments) {
            MONITORINFOEXW info{};info.cbSize=sizeof(info);char device[64]{};
            if(!GetMonitorInfoW(MonitorFromWindow(attachment.window,MONITOR_DEFAULTTONULL),&info))continue;
            WideCharToMultiByte(CP_UTF8,0,info.szDevice,-1,device,64,nullptr,nullptr);
            if(std::strcmp(device,command.device))continue;
            nativeEditTarget=attachment.window;placementReceived=false;placementReply={};
            COPYDATASTRUCT packet{command.statusOnly?placementStatusProtocol:placementProtocol,sizeof(command.request),&command.request};DWORD_PTR accepted{};
            const bool transport=SendMessageTimeoutW(attachment.window,WM_COPYDATA,reinterpret_cast<WPARAM>(owner),reinterpret_cast<LPARAM>(&packet),SMTO_ABORTIFHUNG,3000,&accepted) && accepted && placementReceived;
            nativeEditTarget=nullptr;
            std::printf("slots-report %s status=%08lx bound=%u items=%u passes=%u phase=%u item=%u\n",device,static_cast<unsigned long>(placementReply.status),placementReply.bound,placementReply.items,placementReply.passes,placementReply.failurePhase,placementReply.failureItem);
            const bool ok=transport && (command.statusOnly || SUCCEEDED(placementReply.status));
            std::printf(ok?(command.statusOnly?"slots-status %s\n":"slots-ok %s\n"):"slots-refused %s\n",device);std::fflush(stdout);return ok;
        }
        std::printf("slots-refused %s\n",command.device);std::fflush(stdout);return FALSE;
    }
    if (message == WM_COPYDATA && inspectComponents && snapshotWindow && reinterpret_cast<HWND>(wp) == snapshotWindow) {
        const auto* packet = reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if (!packet || packet->dwData != componentSnapshotProtocol || packet->cbData != sizeof(ComponentSnapshot) || !packet->lpData) return FALSE;
        const auto snapshot = *static_cast<const ComponentSnapshot*>(packet->lpData);
        if (!validComponentSnapshot(snapshot)) return FALSE;
        std::printf("components status=%08lx count=%u visited=%u\n", static_cast<unsigned long>(snapshot.status), snapshot.count, snapshot.visited);
        if (SUCCEEDED(snapshot.status)) for (unsigned i = 0; i < snapshot.count; ++i) {
            auto entry = snapshot.entries[i];
            if (!entry.id[0]) continue;
            entry.id[23] = entry.type[95] = entry.parentType[95] = entry.parentName[79] = 0;
            entry.layoutType[95]=0;
            entry.ownerType[95]=0;
            entry.name[63]=entry.sourceType[63]=0;
            std::printf("  %ls type=%ls parent=%ls/%ls bounds=(%.1f,%.1f,%.1f,%.1f) visible=%u hit=%u movement=unverified\n",
                entry.id, entry.type, entry.parentType, entry.parentName, entry.x, entry.y, entry.width, entry.height,
                entry.visible, entry.hitTestVisible);
            std::printf("    layout=%ls status=%08lx modelIndex=%d\n",entry.layoutType,static_cast<unsigned long>(entry.layoutStatus),entry.modelIndex);
            std::printf("    name=%ls source=%ls\n",entry.name,entry.sourceType);
            std::printf("    sourceStatus=%08lx sourceContracts=%02x\n",static_cast<unsigned long>(entry.sourceStatus),entry.sourceContracts);
            std::printf("    owner=%ls status=%08lx depth=%u containerId=%u ownerId=%u\n",entry.ownerType,static_cast<unsigned long>(entry.ownerStatus),entry.ownerDepth,entry.containerId,entry.ownerId);
        }
        reportReceived = true;
        if (FAILED(snapshot.status) && !inspectFixture) failed=true;
        if (inspectFixture && SUCCEEDED(snapshot.status)) failed=true; // Own fixture has no taskbar XAML host.
        return TRUE;
    }
    if (message == monitorMessage) {
        const auto& request = *reinterpret_cast<const MonitorRequest*>(lp);
        for (size_t i = 0; i < attachments.size(); ++i) {
            MONITORINFOEXW info{}; info.cbSize = sizeof(info);
            char name[64]{};
            if (GetMonitorInfoW(MonitorFromWindow(attachments[i].window, MONITOR_DEFAULTTONULL), &info))
                WideCharToMultiByte(CP_UTF8, 0, info.szDevice, -1, name, 64, nullptr, nullptr);
            if (!std::strcmp(name, request.device)) {
                strcpy_s(pendingMonitor, request.device);
                return ownerProcedure(window, configureMessage, request.width, request.packed | ((i + 1) << 16));
            }
        }
        std::puts("error monitor-unavailable"); std::fflush(stdout); return 0;
    }
    if (message == WM_COPYDATA && nativeEditTarget && reinterpret_cast<HWND>(wp) == nativeEditTarget) {
        const auto* packet = reinterpret_cast<const COPYDATASTRUCT*>(lp);
        if (!packet || packet->dwData != editReportProtocol || packet->cbData != sizeof(EditReport) || !packet->lpData) return FALSE;
        const auto& report = *static_cast<const EditReport*>(packet->lpData);
        if (report.size != sizeof(report) || report.version != 1 || report.ancestors > 8) return FALSE;
        { const std::lock_guard lock(editReportMutex); editReport = report; }
        reportReceived = true;
        return TRUE;
    }
    if(message==trayMessage) {
        char device[64]{};
        if(wp>=24 || lp)return FALSE;
        {const std::lock_guard lock(trayRequestMutex);strcpy_s(device,pendingTrayDevice);pendingTrayDevice[0]=0;}
        if(!device[0])return FALSE;
        for(const auto& item:attachments) {
            MONITORINFOEXW info{};info.cbSize=sizeof(info);char current[64]{};
            if(!GetMonitorInfoW(MonitorFromWindow(item.window,MONITOR_DEFAULTTONULL),&info))continue;
            WideCharToMultiByte(CP_UTF8,0,info.szDevice,-1,current,64,nullptr,nullptr);
            if(std::strcmp(device,current))continue;
            nativeEditTarget=item.window;
            const bool ok=SendMessageW(window,editMessage,static_cast<WPARAM>(EditAction::TrayOrder),wp)!=0;
            std::printf(ok?"tray-ok %s\n":"tray-refused %s\n",device);std::fflush(stdout);return ok;
        }
        std::printf("tray-refused %s\n",device);std::fflush(stdout);return FALSE;
    }
    if (message == editMessage && ((editExperiment && attachments.size()==1) || nativeEditTarget)) {
        EditRequest request; request.action = static_cast<EditAction>(wp); request.position = static_cast<double>(lp);
        COPYDATASTRUCT packet{editProtocol, sizeof(request), &request};
        DWORD_PTR accepted{}; reportReceived = false;
        if(!nativeEditTarget)nativeEditTarget=attachments.front().window;
        // Permit only the bounded reentrant report handler above during this call.
        const bool ok = SendMessageTimeoutW(nativeEditTarget, WM_COPYDATA,
            reinterpret_cast<WPARAM>(owner), reinterpret_cast<LPARAM>(&packet), SMTO_ABORTIFHUNG, 3000, &accepted) && accepted && reportReceived;
        nativeEditTarget=nullptr;
        if (reportReceived) {
            std::printf("taskview status=%08lx edit=%u drag=%u moves=%u clicks=%u focus=%u offset=%.1f button=(%.1f,%.1f,%.1f) parentWidth=%.1f\n",
                static_cast<unsigned long>(editReport.status), editReport.editing, editReport.dragging, editReport.moves,
                editReport.clicks, editReport.focusEvents, editReport.offset, editReport.buttonX, editReport.buttonY,
                editReport.buttonWidth, editReport.parentWidth);
            if (editReport.blockerType[0]) std::printf("  blocker=%ls x=%.1f width=%.1f\n", editReport.blockerType, editReport.blockerX, editReport.blockerWidth);
            std::printf("  input hit=%u rootBackground=%u\n", editReport.hitTestVisible, editReport.rootBackground);
            if (request.action == EditAction::Inspect || request.action == EditAction::Begin)
                for (unsigned i = 0; i < editReport.ancestors; ++i)
                    std::printf("  parent[%u] %ls name=%ls size=%.1fx%.1f\n", i, editReport.chain[i].type,
                        editReport.chain[i].name, editReport.chain[i].width, editReport.chain[i].height);
        }
        std::puts(ok ? "taskview-ok" : "taskview-refused"); std::fflush(stdout);
        if (!ok && editExperiment) failed = true;
        return ok;
    }
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
        const auto target = packed >> 16;
        if (!target) pendingMonitor[0] = 0;
        extendedAck = (packed & (1u << 11)) != 0;
        pendingDetached = detached;
        pendingIslands = islandsMode;
        bool ok = width >= 40 && width <= 100 && edge <= 3 && thickness >= 48 && thickness <= 96;
        if (ok) for (size_t i = 0; i < attachments.size(); ++i) {
            const auto& value = attachments[i];
            if (target && target != i + 1) continue;
            DWORD_PTR suspended{};
            ok = send(value.window, RegisterWindowMessageW(suspendVisualsMessage), protocol, reinterpret_cast<LPARAM>(owner), suspended) && suspended && ok;
        }
        if (ok) for (size_t i = 0; i < attachments.size(); ++i) {
            auto& value = attachments[i];
            if (target && target != i + 1) continue;
            ok = configure(value.window, width, true, edge, thickness, islandsMode, detached) && ok;
            if (ok) { value.islands = islandsMode; value.edge = edge; }
        }
        if (ok) {
            layoutActive = true; currentEdge = edge;
            if (!popupHook && !attachments.empty()) {
                DWORD pid{}; GetWindowThreadProcessId(attachments.front().window, &pid);
                popupHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_LOCATIONCHANGE, nullptr, popupChanged, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
                if (!popupHook) ok = false;
            }
        }
        std::vector<HWND> islandWindows;
        for (const auto& value : attachments) if (value.islands) islandWindows.push_back(value.window);
        islands::setTargets(islandWindows);
        ok = islands::setEnabled(ok && !islandWindows.empty()) && ok;
        if (ok && !islandWindows.empty()) {
            pendingWidth = width; pendingEdge = edge; pendingThickness = thickness; maskPending = true;
            SetTimer(window, 2, 2500, nullptr);
            return 0;
        }
        maskPending = false;
        KillTimer(window, 2);
        std::printf(ok ? "applied %u %u %u %u" : "error %u %u %u %u", width, edge, thickness, islandsMode ? 1u : 0u);
        if (extendedAck) std::printf(" %u", detached ? 1u : 0u);
        if (pendingMonitor[0]) std::printf(" %s", pendingMonitor);
        std::puts("");
        std::fflush(stdout);
        return 0;
    }
    if (message == islands::maskReady || (message == WM_TIMER && wp == 2)) {
        if (maskPending) {
            KillTimer(window, 2); maskPending = false;
            std::printf(message == islands::maskReady && wp ? "applied %u %u %u %u" : "error %u %u %u %u",
                pendingWidth, pendingEdge, pendingThickness, pendingIslands ? 1u : 0u);
            if (extendedAck) std::printf(" %u", pendingDetached ? 1u : 0u);
            if (pendingMonitor[0]) std::printf(" %s", pendingMonitor);
            std::puts("");
            std::fflush(stdout);
        }
        return 0;
    }
    if (message == WM_TIMER || message == WM_CLOSE) {
        if (editExperiment && message == WM_CLOSE && wp == 1) failed = true;
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
        const auto point = popup_placement::position(panel, rectangle, info.rcMonitor, value.edge,
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
    inspectFixture=argc==2 && !wcscmp(argv[1],L"--fixture-inspect-components");
    const bool placementFixture=argc==2 && !wcscmp(argv[1],L"--fixture-placement");
    placementSession=placementFixture || (argc==2 && !wcscmp(argv[1],L"--placement-session"));
    const bool fixture = inspectFixture || placementFixture || (argc == 2 && !wcscmp(argv[1], L"--fixture"));
    const bool serve = argc == 2 && !wcscmp(argv[1], L"--serve");
    const bool inspect = argc == 2 && !wcscmp(argv[1], L"--inspect-taskview");
    inspectComponents = inspectFixture || (argc == 2 && !wcscmp(argv[1], L"--inspect-components"));
    configureTaskview = argc == 2 && !wcscmp(argv[1], L"--configure-taskview");
    verifyOwnerDeath = argc == 2 && !wcscmp(argv[1], L"--verify-taskview-owner-death");
    verifyPlacement = argc == 2 && !wcscmp(argv[1], L"--verify-taskview-placement");
    const bool verify = verifyPlacement || verifyOwnerDeath || (argc == 2 && !wcscmp(argv[1], L"--verify-taskview"));
    editExperiment = inspectComponents || configureTaskview || inspect || verify || (argc == 4 && !wcscmp(argv[1], L"--edit-taskview") && !wcscmp(argv[2], L"--seconds"));
    if ((serve || editExperiment || placementSession) && !inspectComponents && FindWindowExW(HWND_MESSAGE, nullptr, ownerClass, nullptr)) {
        std::puts("FAILED; another Umbrella taskbar session is active"); return 5;
    }
    if ((editExperiment || placementSession) && !fixture && !native_layout::installedBuildSupported()) {
        std::puts("FAILED; taskview experiment requires the exact supported taskbar DLLs"); return 5;
    }
    if (!fixture && !serve && !editExperiment && !placementSession && (argc != 5 || wcscmp(argv[1], L"--width") || wcscmp(argv[3], L"--seconds"))) {
        std::puts("Usage: UmbrellaTaskbarHost --fixture | --fixture-inspect-components | --inspect-components | --serve | --width 40..100 --seconds 1..120 | --inspect-taskview | --edit-taskview --seconds 1..120 | --verify-taskview | --verify-taskview-owner-death");
        return 2;
    }
    unsigned width = fixture ? 75 : serve || editExperiment || placementSession ? 100 : static_cast<unsigned>(_wtoi(argv[2]));
    unsigned seconds = verify ? 120 : fixture || serve || inspect || inspectComponents || configureTaskview || placementSession ? 1 : editExperiment ? static_cast<unsigned>(_wtoi(argv[3])) : static_cast<unsigned>(_wtoi(argv[4]));
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
        testType.lpfnWndProc = inspectFixture ? observerFixtureProcedure : DefWindowProcW;
        RegisterClassW(&testType);
        test = CreateWindowW(testType.lpszClassName, L"Umbrella module geometry test", WS_POPUP,
                             0, 0, GetSystemMetrics(SM_CXSCREEN), 48, nullptr, nullptr, testType.hInstance, nullptr);
        failed = !test || !attach(test);
    } else if (inspectComponents) {
        for (const auto window : umbrella::platform::taskbarWindows()) {
            if (!isExplorer(window) || !attach(window)) { failed = true; break; }
        }
    } else if(placementSession) {
        for(const auto window:umbrella::platform::taskbarWindows())if(!isExplorer(window) || !attach(window)){failed=true;break;}
    } else if (editExperiment) {
        const HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
        RECT rect{}; MONITORINFO monitor{sizeof(monitor)};
        APPBARDATA bar{}; bar.cbSize = sizeof(bar); bar.hWnd = primary;
        HRGN region = CreateRectRgn(0, 0, 0, 0);
        const int regionKind = region ? GetWindowRgn(primary, region) : -1;
        const bool gotRect = GetWindowRect(primary, &rect) != 0;
        const bool gotMonitor = GetMonitorInfoW(MonitorFromWindow(primary, MONITOR_DEFAULTTONULL), &monitor) != 0;
        // Umbrella's ordinary restore can leave an explicit full rectangular region.
        HRGN full = gotRect ? CreateRectRgn(0, 0, rect.right - rect.left, rect.bottom - rect.top) : nullptr;
        const bool unclipped = region && (regionKind == ERROR || (full && EqualRgn(region, full)));
        if (full) DeleteObject(full);
        if (region) DeleteObject(region);
        const bool standard = primary && isExplorer(primary) && unclipped && GetDpiForWindow(primary) == 96 &&
            gotRect && gotMonitor &&
            (monitor.dwFlags & MONITORINFOF_PRIMARY) && rect.left == monitor.rcMonitor.left && rect.right == monitor.rcMonitor.right &&
            rect.bottom == monitor.rcMonitor.bottom && rect.bottom - rect.top == 48 && !(SHAppBarMessage(ABM_GETSTATE, &bar) & ABS_AUTOHIDE);
        if (!standard) {
            std::printf("FAILED; baseline: dpi=%u rect=(%ld,%ld,%ld,%ld) monitor=(%ld,%ld,%ld,%ld) unclipped=%u autohide=%u\n",
                GetDpiForWindow(primary), rect.left, rect.top, rect.right, rect.bottom,
                monitor.rcMonitor.left, monitor.rcMonitor.top, monitor.rcMonitor.right, monitor.rcMonitor.bottom,
                unclipped ? 1u : 0u, (SHAppBarMessage(ABM_GETSTATE, &bar) & ABS_AUTOHIDE) ? 1u : 0u);
            failed = true;
        } else if (!attach(primary)) { std::puts("FAILED; primary taskbar attach handshake refused"); failed = true; }
    } else for (const auto window : umbrella::platform::taskbarWindows()) if (!collect(window, 0)) break;
    if (attachments.empty()) failed = true;
    if (!serve && !editExperiment && !placementSession) for (const auto& value : attachments) {
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
    if (inspectComponents && !failed) {
        for (const auto& value : attachments) {
            snapshotWindow = value.window; reportReceived = false;
            DWORD_PTR accepted{};
            if (!SendMessageTimeoutW(value.window,RegisterWindowMessageW(componentObserveMessage),protocol,
                reinterpret_cast<LPARAM>(owner),SMTO_ABORTIFHUNG,3000,&accepted) || !reportReceived) failed = true;
        }
        snapshotWindow = nullptr;
        restore(); if (inspectFixture && fixtureNativeTouched) failed=true;
        if (test) DestroyWindow(test); DestroyWindow(owner);
    } else if(placementSession && !failed) {
        inputThread=CreateThread(nullptr,0,[](void*)->DWORD {
            char line[1024]{};
            while(std::fgets(line,sizeof(line),stdin)) {
                PlacementCommand command;
                if(!parsePlacementCommand(line,command)){std::puts("slots-refused invalid-request");std::fflush(stdout);break;}
                {const std::lock_guard lock(placementMutex);pendingPlacement=command;}
                SendMessageW(owner,placementMessage,0,0);
            }
            PostMessageW(owner,WM_CLOSE,0,0);return 0;
        },nullptr,0,nullptr);
        if(!inputThread || !SetTimer(owner,1,60000,nullptr)){failed=true;PostMessageW(owner,WM_CLOSE,0,0);}
        else {std::puts("slots-ready");std::fflush(stdout);}
    } else if (editExperiment && !failed) {
        const bool ok = SendMessageW(owner, editMessage, static_cast<WPARAM>(inspect || configureTaskview || verifyPlacement ? EditAction::Inspect : EditAction::Begin), 0) != 0;
        if (inspect || !ok) {
            restore(); DestroyWindow(owner);
        } else if (configureTaskview) {
            std::puts("layout-ready"); std::fflush(stdout);
            inputThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
                char line[64]{};
                while (std::fgets(line, sizeof(line), stdin)) {
                    unsigned x{}; char extra{};
                    if (sscanf_s(line, "position %u %c", &x, &extra, 1u) == 1 && x <= 16384)
                        PostMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::Place), x);
                    else if (sscanf_s(line, "after-widgets %u %c", &x, &extra, 1u) == 1 && x <= 96)
                        PostMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::AfterWidgets), x);
                    else if (sscanf_s(line, "tray-clock-first %u %c", &x, &extra, 1u) == 1 && x<=1)
                        PostMessageW(owner,editMessage,static_cast<WPARAM>(EditAction::TrayClockFirst),x);
                    else if (sscanf_s(line, "tray-order %u %c", &x, &extra, 1u) == 1 && x<24)
                        PostMessageW(owner,editMessage,static_cast<WPARAM>(EditAction::TrayOrder),x);
                    else if (!std::strcmp(line,"taskview-reset\n")) PostMessageW(owner,editMessage,static_cast<WPARAM>(EditAction::ResetTaskView),0);
                    else if (!std::strcmp(line, "status\n")) PostMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::Status), 0);
                    else if (!std::strcmp(line, "reset\n")) PostMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::Reset), 0);
                    else break;
                }
                PostMessageW(owner, WM_CLOSE, 0, 0); return 0;
            }, nullptr, 0, nullptr);
            if (!inputThread) { failed = true; PostMessageW(owner, WM_CLOSE, 1, 0); }
        } else if (verify) {
            inputThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
                const bool ok = button_edit_verification::run(attachments.front().window, [](EditAction action, EditReport& report) {
                    const bool accepted = SendMessageW(owner, editMessage, static_cast<WPARAM>(action), 0) != 0;
                    { const std::lock_guard lock(editReportMutex); report = editReport; }
                    return accepted;
                }, verifyOwnerDeath, verifyPlacement ? std::function<bool(int)>([](int x) {
                    return SendMessageW(owner, editMessage, static_cast<WPARAM>(EditAction::Place), x) != 0;
                }) : std::function<bool(int)>{});
                if (ok && verifyOwnerDeath) {
                    std::puts("verify: ready-for-owner-death; timeout still armed"); std::fflush(stdout);
                    return 0;
                }
                PostMessageW(owner, WM_CLOSE, ok ? 0 : 1, 0);
                return 0;
            }, nullptr, 0, nullptr);
            if (!inputThread || !SetTimer(owner, 1, seconds * 1000, nullptr)) { failed = true; PostMessageW(owner, WM_CLOSE, 0, 0); }
        } else {
            std::puts("Left-drag the original Task View button in edit mode. Use preview to restore native clicks. Commands: status, preview, edit, reset, quit. Automatic rollback at timeout/EOF/host death.");
            std::fflush(stdout);
            inputThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
                char line[32]{};
                while (std::fgets(line, sizeof(line), stdin)) {
                    EditAction action{};
                    if (!std::strcmp(line, "status\n")) action = EditAction::Status;
                    else if (!std::strcmp(line, "preview\n")) action = EditAction::Preview;
                    else if (!std::strcmp(line, "edit\n")) action = EditAction::Begin;
                    else if (!std::strcmp(line, "reset\n")) action = EditAction::Reset;
                    else { PostMessageW(owner, WM_CLOSE, 0, 0); return 0; }
                    PostMessageW(owner, editMessage, static_cast<WPARAM>(action), 0);
                }
                PostMessageW(owner, WM_CLOSE, 0, 0); return 0;
            }, nullptr, 0, nullptr);
            if (!inputThread || !SetTimer(owner, 1, seconds * 1000, nullptr)) { failed = true; PostMessageW(owner, WM_CLOSE, 0, 0); }
        }
    } else if (serve && !failed) {
        std::vector<HWND> windows;
        for (const auto& value : attachments) windows.push_back(value.window);
        if (!islands::start(owner, windows)) { restore(); return 5; }
        inputThread = CreateThread(nullptr, 0, [](void*) -> DWORD {
            char line[192]{};
            while (std::fgets(line, sizeof(line), stdin)) {
                unsigned width{}, edge{}, thickness = 48, mode = 0, detached = 0; char extra{};
                MonitorRequest request;
                if((sscanf_s(line,"tray-order %u %63s %c",&width,request.device,64u,&extra,1u)==2 && width<24) ||
                    (sscanf_s(line,"tray %u %63s %c",&width,request.device,64u,&extra,1u)==2 && width<=1)) {
                    {const std::lock_guard lock(trayRequestMutex);strcpy_s(pendingTrayDevice,request.device);}
                    SendMessageW(owner,trayMessage,width,0);continue;
                }
                const int perMonitor = sscanf_s(line, "%u %u %u %u %u %63s %c", &width, &edge, &thickness, &mode, &detached, request.device, 64u, &extra, 1u);
                if (perMonitor == 6 && width >= 40 && width <= 100 && edge <= 3 && thickness >= (mode ? 64u : 48u) && thickness <= 96 && mode <= 1 && detached <= 1) {
                    request.width = width;
                    request.packed = edge | (thickness << 2) | (mode << 9) | (detached << 10) | (1u << 11);
                    SendMessageW(owner, monitorMessage, 0, reinterpret_cast<LPARAM>(&request));
                    continue;
                }
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
    std::puts(failed ? (inspectComponents ? "FAILED; read-only observation ended; no restore sent" : "FAILED; restore requested") : inspectComponents ? "Passed; read-only component snapshot; observer hook removed; diagnostic image retained until process exit" : editExperiment ? "Passed; taskview reset acknowledged; native session ended" : "Passed; original geometry restored");
    return failed ? 1 : 0;
}
