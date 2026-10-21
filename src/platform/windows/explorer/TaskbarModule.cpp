#include "TaskbarModuleProtocol.h"
#include "NativeTaskbarLayout.h"
#include "TaskbarContextMenu.h"
#include "TaskbarVisualLayout.h"
#include "TaskbarSmoothClip.h"
#include "TaskbarPopupPlacement.h"
#include "TaskbarButtonEdit.h"
#include "TaskbarComponentSnapshot.h"
#include "TaskbarTrayOrder.h"
#include <shellapi.h>
#include <CommCtrl.h>
#include <array>
#include <algorithm>
#include <new>
#include <cstring>

namespace {
using namespace umbrella::explorer;
constexpr UINT_PTR subclassId = protocol;
struct State {
    HWND window{};
    std::array<HWND, 16> popups{};
    visual_layout::Session visuals;
    smooth_clip::Session smooth;
    button_edit::Session buttonEdit;
    tray_order::Session trayOrder;
    bool detached = false;
    SIZE clipSize{};
    UINT clipDpi{};
    HWND owner{};
    HANDLE ownerProcess{};
    HANDLE stop{};
    HANDLE worker{};
    RECT original{};
    HMONITOR originalMonitor{};
    unsigned width = 100;
    unsigned thickness = 48;
    bool islands = false;
    bool regionSaved = false;
    HRGN originalRegion{};
    IslandMask lastMask{};
    bool maskApplied = false;
    bool enabled = false;
    bool restoring = false;
    bool nativeLayout = false;
    bool forceGeometry = false;
    unsigned edge = 3;
    unsigned originalEdge = 3;
};
thread_local std::array<State*, 16> roots{};
struct Watch {
    HWND target{};
    HANDLE owner{};
    HANDLE stop{};
    HMODULE module{};
};
bool saveRegion(State* state, HWND hwnd) {
    HRGN region = CreateRectRgn(0, 0, 0, 0);
    if (!region) return false;
    if (GetWindowRgn(hwnd, region) == ERROR) { DeleteObject(region); region = nullptr; }
    state->originalRegion = region;
    state->regionSaved = true;
    return true;
}
bool taskbarWindow(HWND hwnd) {
    wchar_t name[64]{};
    return GetClassNameW(hwnd, name, 64) &&
        (!wcscmp(name, L"Shell_TrayWnd") || !wcscmp(name, L"Shell_SecondaryTrayWnd") ||
         !wcscmp(name, L"Umbrella.Taskbar.ModuleFixture.1"));
}
bool trustedOwner(HWND owner, HANDLE& process) {
    wchar_t name[64]{};
    if (!GetClassNameW(owner, name, 64) || wcscmp(name, ownerClass)) return false;
    DWORD pid{};
    GetWindowThreadProcessId(owner, &pid);
    process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> ownerPath{}, dllPath{};
    DWORD count = static_cast<DWORD>(ownerPath.size());
    HMODULE module{};
    bool ok = QueryFullProcessImageNameW(process, 0, ownerPath.data(), &count) &&
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&trustedOwner), &module) &&
        GetModuleFileNameW(module, dllPath.data(), static_cast<DWORD>(dllPath.size()));
    if (ok) {
        auto* end = wcsrchr(dllPath.data(), L'\\');
        if (!end) ok = false;
        else {
            *(end + 1) = 0;
            ok = wcscat_s(dllPath.data(), dllPath.size(), L"UmbrellaTaskbarHost.exe") == 0 &&
                _wcsicmp(ownerPath.data(), dllPath.data()) == 0;
        }
    }
    if (!ok) { CloseHandle(process); process = nullptr; }
    return ok;
}
DWORD WINAPI watchOwner(void* parameter) {
    const auto value = *static_cast<Watch*>(parameter);
    delete static_cast<Watch*>(parameter);
    const HANDLE waits[]{value.stop, value.owner};
    const DWORD reason = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    DWORD_PTR result{};
    // A synchronous round trip waits until the UI callback has returned before unloading.
    const UINT message = reason == WAIT_OBJECT_0 + 1 ? RegisterWindowMessageW(restoreMessage) : WM_NULL;
    const bool returned = reason != WAIT_FAILED && SendMessageTimeoutW(value.target, message, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &result);
    CloseHandle(value.owner);
    CloseHandle(value.stop);
    if (returned || !IsWindow(value.target)) FreeLibraryAndExitThread(value.module, 0);
    // A hung callback may still execute this DLL. Keep this one reference until process exit.
    return 1;
}
void positionPopup(HWND hwnd, State* state, WINDOWPOS* value) {
    if (!state || !state->enabled || state->restoring) return;
    RECT popup{}, panel{}; MONITORINFO monitor{sizeof(monitor)};
    if (!GetWindowRect(hwnd, &popup) || !GetWindowRect(state->window, &panel) || !GetMonitorInfoW(state->originalMonitor, &monitor)) return;
    if (!(value->flags & SWP_NOMOVE)) { const LONG w = popup.right - popup.left, h = popup.bottom - popup.top; popup = {value->x, value->y, value->x + w, value->y + h}; }
    if (!(value->flags & SWP_NOSIZE)) { popup.right = popup.left + value->cx; popup.bottom = popup.top + value->cy; }
    if (popup.right <= popup.left || popup.bottom <= popup.top) return;
    const auto point = popup_placement::position(panel, popup, monitor.rcMonitor, state->edge,
        MulDiv(8, static_cast<int>(GetDpiForWindow(state->window)), 96));
    value->x = point.x; value->y = point.y; value->flags &= ~SWP_NOMOVE;
}
LRESULT CALLBACK popupSubclass(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR reference) {
    auto* state = reinterpret_cast<State*>(reference);
    if (message == WM_NCDESTROY) {
        for (auto& window : state->popups) if (window == hwnd) window = nullptr;
        RemovePropW(hwnd, popup_placement::ownerProperty);
        RemoveWindowSubclass(hwnd, popupSubclass, subclassId + 1);
        return DefSubclassProc(hwnd, message, wp, lp);
    }
    if (message == WM_WINDOWPOSCHANGING && state->enabled && !state->restoring) {
        const auto result = DefSubclassProc(hwnd, message, wp, lp);
        positionPopup(hwnd, state, reinterpret_cast<WINDOWPOS*>(lp));
        return result;
    }
    return DefSubclassProc(hwnd, message, wp, lp);
}
void attachPopup(HWND window) {
    if (!popup_placement::overflow(window)) return;
    DWORD_PTR installed{};
    if (GetWindowSubclass(window, popupSubclass, subclassId + 1, &installed)) return;
    RECT rectangle{}; if (!GetWindowRect(window, &rectangle)) return;
    const auto monitor = MonitorFromRect(&rectangle, MONITOR_DEFAULTTONEAREST);
    State* target{};
    for (auto* state : roots) if (state && state->enabled && !state->restoring && state->originalMonitor == monitor) { target = state; break; }
    if (!target) return;
    for (auto& popup : target->popups) if (!popup) {
        if (SetWindowSubclass(window, popupSubclass, subclassId + 1, reinterpret_cast<DWORD_PTR>(target))) {
            popup = window;
            SetPropW(window, popup_placement::ownerProperty, target->owner);
        }
        break;
    }
}
void detachPopups(State* state) {
    for (auto& window : state->popups) if (window) {
        RemovePropW(window, popup_placement::ownerProperty);
        RemoveWindowSubclass(window, popupSubclass, subclassId + 1);
        if (IsWindowVisible(window)) {
            RECT rectangle{}; MONITORINFO monitor{sizeof(monitor)};
            if (GetWindowRect(window, &rectangle) && GetMonitorInfoW(state->originalMonitor, &monitor)) {
                const auto point = popup_placement::position(state->original, rectangle, monitor.rcMonitor, state->originalEdge,
                    MulDiv(8, static_cast<int>(GetDpiForWindow(state->window)), 96));
                SetWindowPos(window, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
            }
        }
        window = nullptr;
    }
}
void dispose(State* state) {
    detachPopups(state);
    for (auto& root : roots) if (root == state) root = nullptr;
    if (state->stop) SetEvent(state->stop);
    if (state->worker) CloseHandle(state->worker);
    if (state->stop) CloseHandle(state->stop);
    if (state->ownerProcess) CloseHandle(state->ownerProcess);
    if (state->nativeLayout) native_layout::release();
    if (state->originalRegion) DeleteObject(state->originalRegion);
    delete state;
}
bool updateDetachedClip(HWND hwnd, State* state) {
    if (state->islands || state->restoring) return true;
    RECT window{};
    if (!GetWindowRect(hwnd, &window)) return false;
    MONITORINFO monitor{sizeof(monitor)};
    const bool visible = GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor) &&
        window.left >= monitor.rcMonitor.left && window.top >= monitor.rcMonitor.top &&
        window.right <= monitor.rcMonitor.right && window.bottom <= monitor.rcMonitor.bottom;
    if (!state->detached || !visible) { state->smooth.reset(); state->clipSize = {}; return true; }
    const LONG width = window.right - window.left, height = window.bottom - window.top;
    const UINT dpi = GetDpiForWindow(hwnd);
    if (state->clipSize.cx == width && state->clipSize.cy == height && state->clipDpi == dpi) return true;
    const int gap = MulDiv(6, static_cast<int>(dpi), 96);
    const RECT rectangle{gap, gap, width - gap, height - gap};
    if (!state->smooth.apply(hwnd, &rectangle, 1, 9.0f)) return false;
    state->clipSize = {width, height}; state->clipDpi = dpi;
    return true;
}
void suspendAllVisuals(HWND owner) {
    for (const auto* name : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
        HWND window{};
        for (unsigned count = 0; count < 16 && (window = FindWindowExW(nullptr, window, name, nullptr)); ++count) {
            DWORD_PTR ignored{};
            SendMessageTimeoutW(window, RegisterWindowMessageW(suspendVisualsMessage), protocol,
                reinterpret_cast<LPARAM>(owner), SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &ignored);
        }
    }
}
LRESULT CALLBACK subclass(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                          UINT_PTR, DWORD_PTR reference) {
    auto* state = reinterpret_cast<State*>(reference);
    if (message == RegisterWindowMessageW(suspendVisualsMessage) && wparam == protocol &&
        reinterpret_cast<HWND>(lparam) == state->owner) {
        state->visuals.suspend(); return TRUE;
    }
    if (message == RegisterWindowMessageW(restoreMessage) || message == WM_NCDESTROY) {
        const bool trayRestored=state->trayOrder.reset();
        const bool buttonRestored=state->buttonEdit.reset(); // Includes owner-death recovery before DLL teardown.
        state->restoring = true;
        detachPopups(state);
        suspendAllVisuals(state->owner);
        state->visuals.reset();
        state->smooth.reset();
        if (state->nativeLayout && message != WM_NCDESTROY && FindWindowW(L"Shell_TrayWnd", nullptr) == hwnd) {
            native_layout::setEdge(state->originalEdge);
            DWORD_PTR changed{};
            SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"TraySettings"),
                                SMTO_ABORTIFHUNG, 100, &changed);
            SendMessageW(hwnd, 0x5ca, 6, state->originalEdge);
        }
        if (state->regionSaved && message != WM_NCDESTROY) {
            HRGN copy = nullptr;
            if (state->originalRegion) {
                copy = CreateRectRgn(0, 0, 0, 0);
                if (copy) CombineRgn(copy, state->originalRegion, nullptr, RGN_COPY);
            }
            if (!SetWindowRgn(hwnd, copy, TRUE) && copy) DeleteObject(copy);
        }
        RemoveWindowSubclass(hwnd, subclass, subclassId);
        if (message != WM_NCDESTROY && state->enabled)
            SetWindowPos(hwnd, nullptr, state->original.left, state->original.top,
                         state->original.right - state->original.left,
                         state->original.bottom - state->original.top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        dispose(state);
        if(message!=WM_NCDESTROY)return trayRestored && buttonRestored ? TRUE : FALSE;
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    if (message == WM_COPYDATA && reinterpret_cast<HWND>(wparam) == state->owner) {
        const auto* packet = reinterpret_cast<const COPYDATASTRUCT*>(lparam);
        if (packet && packet->dwData == componentInspectProtocol && packet->cbData == sizeof(Configuration) && packet->lpData) {
            const auto& request = *static_cast<const Configuration*>(packet->lpData);
            if (request.size != sizeof(Configuration) || request.version != 3 || request.enabled) return FALSE;
            auto snapshot = components::inspect(hwnd);
            COPYDATASTRUCT reply{componentSnapshotProtocol, sizeof(snapshot), &snapshot};
            DWORD_PTR received{};
            return SendMessageTimeoutW(state->owner, WM_COPYDATA, reinterpret_cast<WPARAM>(hwnd),
                reinterpret_cast<LPARAM>(&reply), SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &received) && received && SUCCEEDED(snapshot.status);
        }
        if (packet && packet->dwData == editProtocol && packet->cbData == sizeof(EditRequest) && packet->lpData) {
            const auto request = *static_cast<const EditRequest*>(packet->lpData);
            if (request.size != sizeof(request) || request.version != 1 || request.action > EditAction::ResetTaskView) return FALSE;
            HRESULT status = S_OK;
            if (request.action == EditAction::Begin) {
                // Experiment never combines with resizing, islands or native edge changes.
                status = state->enabled ? E_ACCESSDENIED : state->buttonEdit.begin(hwnd);
            } else if (request.action == EditAction::Place) {
                status = state->enabled ? E_ACCESSDENIED : state->buttonEdit.position(hwnd, request.position);
            } else if (request.action == EditAction::AfterWidgets) {
                status = state->enabled ? E_ACCESSDENIED : state->buttonEdit.afterWidgets(hwnd, request.position);
            } else if (request.action == EditAction::TrayClockFirst) {
                status=(request.position!=0 && request.position!=1) ? E_ACCESSDENIED : state->trayOrder.clockBeforeQuickSettings(hwnd,request.position==1);
            } else if (request.action == EditAction::ResetTaskView) {
                if(!state->buttonEdit.reset())status=E_FAIL;
            } else if (request.action == EditAction::Preview && !state->buttonEdit.preview()) status = E_FAIL;
            else if (request.action == EditAction::Reset) {
                const bool tray=state->trayOrder.reset(), button=state->buttonEdit.reset();
                if(!tray || !button)status=E_FAIL;
            }
            EditReport report;
            if(request.action==EditAction::TrayClockFirst)report.status=status;
            else report=state->buttonEdit.report(hwnd,status);
            COPYDATASTRUCT reply{editReportProtocol, sizeof(report), &report};
            DWORD_PTR received{};
            return SendMessageTimeoutW(state->owner, WM_COPYDATA, reinterpret_cast<WPARAM>(hwnd),
                reinterpret_cast<LPARAM>(&reply), SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &received) && received && SUCCEEDED(report.status);
        }
        if (packet && packet->dwData == islandsProtocol && packet->cbData == sizeof(IslandMask) && packet->lpData) {
            if (!state->islands) return FALSE;
            const auto& mask = *static_cast<const IslandMask*>(packet->lpData);
            RECT now{};
            if (!mask.count || mask.count > maxIslands || !GetWindowRect(hwnd, &now) ||
                !EqualRect(&now, &mask.bounds)) return FALSE; // Reject stale layout snapshots.
            if (state->maskApplied && !std::memcmp(&mask, &state->lastMask, sizeof(mask))) return TRUE;
            HRGN combined = CreateRectRgn(0, 0, 0, 0);
            if (!combined) return FALSE;
            bool valid = true;
            const int gap = MulDiv(2, static_cast<int>(GetDpiForWindow(hwnd)), 96);
            std::array<RECT, maxIslands> rectangles{};
            for (unsigned i = 0; i < mask.count; ++i) {
                RECT r = mask.rectangles[i];
                const int inset = (std::min)(gap, static_cast<int>((r.right - r.left) / 5));
                InflateRect(&r, -inset, -gap);
                if (r.left < 0 || r.top < 0 || r.right > now.right - now.left ||
                    r.bottom > now.bottom - now.top || r.right <= r.left || r.bottom <= r.top) { valid = false; break; }
                rectangles[i] = r;
                HRGN part = CreateRectRgn(r.left, r.top, r.right, r.bottom);
                if (!part) { valid = false; break; }
                if (CombineRgn(combined, combined, part, RGN_OR) == ERROR) valid = false;
                DeleteObject(part);
                if (!valid) break;
            }
            if (!valid || !state->smooth.apply(hwnd, rectangles.data(), mask.count, 12.0f) || !SetWindowRgn(hwnd, combined, TRUE)) { DeleteObject(combined); return FALSE; }
            state->lastMask = mask; state->maskApplied = true;
            return TRUE;
        }
        if (packet && packet->dwData == protocol && packet->cbData == sizeof(Configuration) && packet->lpData) {
            const auto config = *static_cast<const Configuration*>(packet->lpData);
            if (config.size != sizeof(config) || config.version != 3 || config.widthPercent < 40 ||
                config.widthPercent > 100 || config.enabled > 1 || config.edge > 3 ||
                config.thickness < (config.islands ? 64u : 48u) || config.thickness > 96 || config.islands > 1 || config.detached > 1) return FALSE;
            if (!config.enabled) return TRUE; // Attach handshake must not change Windows.
            if (!state->enabled) state->visuals.prepare(hwnd);
            RECT rect{};
            GetWindowRect(hwnd, &rect);
            const bool vertical = config.edge == 0 || config.edge == 2;
            const bool edgeChanged = state->edge != config.edge;
            const bool firstNativeLayout = vertical && !state->nativeLayout;
            if (vertical || state->nativeLayout) {
                if (!state->nativeLayout) {
                    state->nativeLayout = native_layout::acquire();
                    if (!state->nativeLayout) return FALSE;
                }
                if ((edgeChanged || firstNativeLayout) && !native_layout::setEdge(config.edge)) return FALSE;
            }
            if (config.islands && !state->regionSaved) {
                HRGN saved = CreateRectRgn(0, 0, 0, 0);
                if (!saved) return FALSE;
                if (GetWindowRgn(hwnd, saved) == ERROR) { DeleteObject(saved); saved = nullptr; }
                state->originalRegion = saved;
                state->regionSaved = true;
            }
            if (!config.islands && state->islands) { SetWindowRgn(hwnd, nullptr, TRUE); state->maskApplied = false; }
            state->islands = config.islands != 0;
            state->detached = config.detached != 0;
            state->clipSize = {};
            const bool visualsChanged = !state->enabled || edgeChanged || state->thickness != config.thickness;
            APPBARDATA shared{}; shared.cbSize = sizeof(shared);
            const bool gotSharedEdge = SHAppBarMessage(ABM_GETTASKBARPOS, &shared) != 0;
            const bool forceGeometry = config.edge == 1 || (FindWindowW(L"Shell_TrayWnd", nullptr) != hwnd && gotSharedEdge && config.edge != shared.uEdge);
            const bool geometryChanged = visualsChanged || state->width != config.widthPercent || state->forceGeometry != forceGeometry;
            state->forceGeometry = forceGeometry;
            if (visualsChanged) state->visuals.suspend();
            state->thickness = config.thickness;
            state->edge = config.edge;
            state->width = config.widthPercent;
            state->enabled = true;
            if ((edgeChanged || firstNativeLayout) && FindWindowW(L"Shell_TrayWnd", nullptr) == hwnd) {
                DWORD_PTR changed{};
                if (state->nativeLayout) SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                    reinterpret_cast<LPARAM>(L"TraySettings"), SMTO_ABORTIFHUNG, 100, &changed);
                SendMessageW(hwnd, 0x5ca, 6, config.edge);
            }
            if (geometryChanged) {
                if (!vertical && config.widthPercent == 100) {
                    MONITORINFO monitor{sizeof(monitor)};
                    if (!GetMonitorInfoW(state->originalMonitor, &monitor)) return FALSE;
                    const int thickness = MulDiv(static_cast<int>(state->thickness), static_cast<int>(GetDpiForWindow(hwnd)), 96);
                    SetWindowPos(hwnd, nullptr, monitor.rcMonitor.left,
                        config.edge == 1 ? monitor.rcMonitor.top : monitor.rcMonitor.bottom - thickness,
                        monitor.rcMonitor.right - monitor.rcMonitor.left, thickness, SWP_NOACTIVATE | SWP_NOZORDER);
                } else SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
            }
            if (config.edge == 3 && state->nativeLayout) {
                // Return to the standard bottom path so the normal autohide API remains available.
                state->nativeLayout = false;
                native_layout::release();
            }
            if (!state->visuals.apply(hwnd, config.edge, config.thickness)) return FALSE;
            if (!state->islands && !updateDetachedClip(hwnd, state)) return FALSE;
            return TRUE;
        }
        return FALSE;
    }
    if (message == WM_WINDOWPOSCHANGING && state->enabled && !state->restoring &&
        !(state->edge == 3 && state->width == 100 && state->thickness == 48 && !state->forceGeometry)) {
        const LRESULT result = DefSubclassProc(hwnd, message, wparam, lparam);
        auto* position = reinterpret_cast<WINDOWPOS*>(lparam);
        MONITORINFO monitor{sizeof(monitor)};
        HMONITOR targetMonitor = state->originalMonitor;
        if (!GetMonitorInfoW(targetMonitor, &monitor)) {
            targetMonitor = MonitorFromRect(&state->original, MONITOR_DEFAULTTONEAREST);
        }
        if (GetMonitorInfoW(targetMonitor, &monitor)) {
            const bool vertical = state->edge == 0 || state->edge == 2;
            const int full = vertical ? monitor.rcMonitor.bottom - monitor.rcMonitor.top : monitor.rcMonitor.right - monitor.rcMonitor.left;
            const int extent = MulDiv(full, static_cast<int>(state->width), 100);
            RECT current{};
            GetWindowRect(hwnd, &current);
            if (position->flags & SWP_NOMOVE) position->y = current.top;
            if (position->flags & SWP_NOSIZE) position->cy = current.bottom - current.top;
            if (vertical) {
                const int thickness = MulDiv(static_cast<int>(state->thickness), static_cast<int>(GetDpiForWindow(hwnd)), 96);
                const int aligned = state->edge == 0 ? monitor.rcMonitor.left : monitor.rcMonitor.right - thickness;
                position->x = aligned;
                position->y = monitor.rcMonitor.top + (full - extent) / 2;
                position->cx = thickness;
                position->cy = extent;
            } else {
                const int thickness = MulDiv(static_cast<int>(state->thickness), static_cast<int>(GetDpiForWindow(hwnd)), 96);
                position->x = monitor.rcMonitor.left + (full - extent) / 2;
                const int aligned = state->edge == 1 ? monitor.rcMonitor.top : monitor.rcMonitor.bottom - thickness;
                position->y = aligned;
                position->cx = extent;
                position->cy = thickness;
            }
            if (position->x == current.left && position->y == current.top) position->flags |= SWP_NOMOVE;
            else position->flags &= ~SWP_NOMOVE;
            if (position->cx == current.right - current.left && position->cy == current.bottom - current.top) position->flags |= SWP_NOSIZE;
            else position->flags &= ~SWP_NOSIZE;
        }
        return result;
    }
    if (message == WM_WINDOWPOSCHANGED && state->enabled && !state->restoring) {
        const auto result = DefSubclassProc(hwnd, message, wparam, lparam);
        updateDetachedClip(hwnd, state);
        return result;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}
}

extern "C" __declspec(dllexport) LRESULT CALLBACK UmbrellaTaskbarHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code >= 0) {
        const auto* call = reinterpret_cast<const CWPSTRUCT*>(lparam);
        if (call && (call->message == WM_WINDOWPOSCHANGING || call->message == WM_CREATE || call->message == WM_SHOWWINDOW)) {
            attachPopup(call->hwnd);
            DWORD_PTR reference{};
            if (call->message == WM_WINDOWPOSCHANGING && GetWindowSubclass(call->hwnd, popupSubclass, subclassId + 1, &reference))
                positionPopup(call->hwnd, reinterpret_cast<State*>(reference), reinterpret_cast<WINDOWPOS*>(call->lParam));
        }
        if (call && call->message == RegisterWindowMessageW(umbrella::explorer::attachMessage) &&
            call->wParam == umbrella::explorer::protocol && taskbarWindow(call->hwnd)) {
            DWORD_PTR existing{};
            if (!GetWindowSubclass(call->hwnd, subclass, subclassId, &existing)) {
                auto* state = new(std::nothrow) State;
                if (state) {
                    state->owner = reinterpret_cast<HWND>(call->lParam);
                    state->window = call->hwnd;
                    if (trustedOwner(state->owner, state->ownerProcess) && GetWindowRect(call->hwnd, &state->original) && saveRegion(state, call->hwnd)) {
                        state->originalMonitor = MonitorFromWindow(call->hwnd, MONITOR_DEFAULTTONEAREST);
                        APPBARDATA appbar{}; appbar.cbSize = sizeof(appbar);
                        appbar.hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
                        if (SHAppBarMessage(ABM_GETTASKBARPOS, &appbar)) state->originalEdge = appbar.uEdge;
                        state->edge = state->originalEdge;
                        state->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                        auto* watch = new(std::nothrow) Watch;
                        bool prepared = state->stop && watch;
                        if (prepared) {
                            watch->target = call->hwnd;
                            prepared = DuplicateHandle(GetCurrentProcess(), state->ownerProcess, GetCurrentProcess(), &watch->owner, 0, FALSE, DUPLICATE_SAME_ACCESS)
                                && DuplicateHandle(GetCurrentProcess(), state->stop, GetCurrentProcess(), &watch->stop, 0, FALSE, DUPLICATE_SAME_ACCESS);
                        }
                        if (prepared && SetWindowSubclass(call->hwnd, subclass, subclassId, reinterpret_cast<DWORD_PTR>(state))) {
                            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                    reinterpret_cast<LPCWSTR>(&UmbrellaTaskbarHook), &watch->module))
                                state->worker = CreateThread(nullptr, 0, watchOwner, watch, 0, nullptr);
                            if (state->worker) {
                                state->visuals.prepare(call->hwnd);
                                for (auto& root : roots) if (!root) { root = state; break; }
                                state = nullptr;
                            }
                            else RemoveWindowSubclass(call->hwnd, subclass, subclassId);
                        }
                        if (state && watch) {
                            if (watch->owner) CloseHandle(watch->owner);
                            if (watch->stop) CloseHandle(watch->stop);
                            if (watch->module) FreeLibrary(watch->module);
                            delete watch;
                        }
                    }
                    if (state) dispose(state);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

// Separate entry point in the SAME module: no State, subclass, geometry,
// popup attachment or restore message. May coexist with an appearance session.
extern "C" __declspec(dllexport) LRESULT CALLBACK UmbrellaTaskbarInspectHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code>=0) {
        const auto* call=reinterpret_cast<const CWPSTRUCT*>(lparam);
        if (call && call->message==RegisterWindowMessageW(componentObserveMessage) &&
            call->wParam==protocol && taskbarWindow(call->hwnd)) {
            const auto observer=reinterpret_cast<HWND>(call->lParam);
            HANDLE process{};
            if (trustedOwner(observer,process)) {
                CloseHandle(process);
                // A timed-out SendMessage can still be executing on this thread
                // after the host unhooks. Retain this diagnostic image once per
                // Explorer process, so that late completion cannot unload code.
                // No workers, subscriptions or XAML refs survive the callback.
                static const bool retained=[] {
                    HMODULE image{};
                    return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                        reinterpret_cast<LPCWSTR>(&UmbrellaTaskbarInspectHook),&image)!=FALSE;
                }();
                if (retained) {
                    const auto snapshot=components::inspect(call->hwnd);
                    COPYDATASTRUCT packet{componentSnapshotProtocol,sizeof(snapshot),const_cast<ComponentSnapshot*>(&snapshot)};
                    DWORD_PTR accepted{};
                    SendMessageTimeoutW(observer,WM_COPYDATA,reinterpret_cast<WPARAM>(call->hwnd),
                        reinterpret_cast<LPARAM>(&packet),SMTO_ABORTIFHUNG,1000,&accepted);
                }
            }
        }
    }
    return CallNextHookEx(nullptr,code,wparam,lparam);
}
