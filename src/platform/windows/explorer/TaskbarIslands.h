#pragma once
#include "TaskbarModuleProtocol.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <array>
#include <cstdio>
#include <atomic>
#include <algorithm>
#include <vector>

// UIA calls run outside Explorer and outside the host's recovery/message thread.
// No text, accessible names or application identities are requested or retained.
namespace umbrella::explorer::islands {
using Microsoft::WRL::ComPtr;
inline HANDLE thread{};
inline DWORD threadId{};
inline std::atomic_bool enabled{false};
inline std::vector<HWND> targets;
inline UINT refreshMessage = WM_APP + 31;
inline bool pending = false;
inline HWND ownerWindow{};
inline unsigned long snapshots = 0;
inline HANDLE queueReady{};
inline std::atomic_bool report{false};
inline std::atomic_uint retries{0};
inline constexpr UINT maskReady = WM_APP + 33;
inline void refresh(IUIAutomation* automation) {
    if (!enabled || !automation) return;
    ComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(automation->get_ControlViewWalker(&walker))) return;
    bool complete = true;
    for (HWND window : targets) {
        IslandMask mask;
        std::array<RECT, 3> groups{};
        std::array<bool, 3> present{};
        if (!GetWindowRect(window, &mask.bounds)) { complete = false; continue; }
        ComPtr<IUIAutomationElement> root;
        if (FAILED(automation->ElementFromHandle(window, &root))) { complete = false; continue; }
        std::vector<ComPtr<IUIAutomationElement>> stack;
        stack.push_back(root);
        unsigned visited = 0;
        while (!stack.empty() && visited++ < 512 && mask.count < maxIslands) {
            auto element = stack.back(); stack.pop_back();
            CONTROLTYPEID type{}; BOOL offscreen = TRUE;
            RECT rect{};
            if (SUCCEEDED(element->get_CurrentControlType(&type)) && type == UIA_ButtonControlTypeId &&
                SUCCEEDED(element->get_CurrentIsOffscreen(&offscreen)) && !offscreen &&
                SUCCEEDED(element->get_CurrentBoundingRectangle(&rect))) {
                RECT clipped{};
                if (IntersectRect(&clipped, &rect, &mask.bounds) && clipped.right - clipped.left >= 6 && clipped.bottom - clipped.top >= 12) {
                    OffsetRect(&clipped, -mask.bounds.left, -mask.bounds.top);
                    unsigned group = 1; // Start, search, task view and applications share one island.
                    BSTR className{};
                    if (SUCCEEDED(element->get_CurrentClassName(&className)) && className) {
                        if (!wcsncmp(className, L"SystemTray.", 11)) group = 2;
                        else if (!wcscmp(className, L"ToggleButton")) {
                            BSTR identifier{};
                            if (SUCCEEDED(element->get_CurrentAutomationId(&identifier)) && identifier &&
                                !wcscmp(identifier, L"WidgetsButton")) group = 0;
                            SysFreeString(identifier);
                        }
                    }
                    SysFreeString(className);
                    if (present[group]) {
                        RECT combined{}; UnionRect(&combined, &groups[group], &clipped); groups[group] = combined;
                    } else { groups[group] = clipped; present[group] = true; }
                }
                continue; // A composite tray button owns its icon/text descendants.
            }
            ComPtr<IUIAutomationElement> child;
            if (SUCCEEDED(walker->GetFirstChildElement(element.Get(), &child))) {
                while (child && stack.size() < 512) {
                    stack.push_back(child);
                    ComPtr<IUIAutomationElement> next;
                    if (FAILED(walker->GetNextSiblingElement(child.Get(), &next))) break;
                    child = next;
                }
            }
        }
        const bool vertical = mask.bounds.bottom - mask.bounds.top > mask.bounds.right - mask.bounds.left;
        const int width = mask.bounds.right - mask.bounds.left, height = mask.bounds.bottom - mask.bounds.top;
        const int padding = MulDiv(8, static_cast<int>(GetDpiForWindow(window)), 96);
        const int margin = MulDiv(5, static_cast<int>(GetDpiForWindow(window)), 96);
        for (unsigned group = 0; group < 3; ++group) if (present[group]) {
            RECT r = groups[group];
            if (vertical) { r.top = (std::max)(margin, static_cast<int>(r.top) - padding); r.bottom = (std::min)(height - margin, static_cast<int>(r.bottom) + padding); r.left = margin; r.right = width - margin; }
            else { r.left = (std::max)(margin, static_cast<int>(r.left) - padding); r.right = (std::min)(width - margin, static_cast<int>(r.right) + padding); r.top = margin; r.bottom = height - margin; }
            if (r.right > r.left && r.bottom > r.top) mask.rectangles[mask.count++] = r;
        }
        std::sort(mask.rectangles, mask.rectangles + mask.count, [vertical](const RECT& a, const RECT& b) {
            return vertical ? a.top < b.top : a.left < b.left;
        });
        const int separation = MulDiv(8, static_cast<int>(GetDpiForWindow(window)), 96);
        for (unsigned i = 1; i < mask.count; ++i) {
            auto& before = mask.rectangles[i - 1]; auto& after = mask.rectangles[i];
            LONG& end = vertical ? before.bottom : before.right;
            LONG& start = vertical ? after.top : after.left;
            if (start - end < separation) {
                const LONG middle = (start + end) / 2;
                end = middle - separation / 2; start = middle + separation / 2;
            }
        }
        if (report) std::fprintf(stderr, "Island snapshot: nodes=%u groups=%u\n", visited, mask.count);
        if (!enabled || !mask.count || visited >= 512) { complete = false; continue; } // Never hide the whole taskbar on a provider failure.
        COPYDATASTRUCT packet{islandsProtocol, sizeof(mask), &mask};
        DWORD_PTR result{};
        // The receiver accepts only its authenticated owner and a current rectangle.
        extern HWND ownerWindow;
        const bool sent = SendMessageTimeoutW(window, WM_COPYDATA, reinterpret_cast<WPARAM>(ownerWindow),
            reinterpret_cast<LPARAM>(&packet), SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &result) != 0;
        if (report) std::fprintf(stderr, "Island mask: sent=%u accepted=%u\n", sent ? 1u : 0u, result ? 1u : 0u);
        complete = complete && sent && result;
    }
    ++snapshots;
    if (report && complete) {
        report = false;
        PostMessageW(ownerWindow, maskReady, 1, 0);
    } else if (report && retries++ < 8 && enabled) {
        pending = true;
        SetTimer(nullptr, 1, 180, nullptr); // Bounded settling after Windows rebuilds its UIA tree.
    } else if (report.exchange(false)) PostMessageW(ownerWindow, maskReady, 0, 0);
}
inline void CALLBACK changed(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) {
    if (!enabled || !window || pending) return;
    HWND root = GetAncestor(window, GA_ROOT);
    bool ours = false;
    for (HWND target : targets) if (window == target || root == target || IsChild(target, window)) ours = true;
    if (ours) PostThreadMessageW(threadId, refreshMessage, 0, 0);
}
inline DWORD WINAPI run(void*) {
    MSG initial{}; PeekMessageW(&initial, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    SetEvent(queueReady);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) return 1;
    ComPtr<IUIAutomation> automation;
    CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation));
    // Scope events to the Explorer process; delivery is on this worker's message loop.
    DWORD pid{};
    if (!targets.empty()) GetWindowThreadProcessId(targets.front(), &pid);
    HWINEVENTHOOK hook = pid ? SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
        changed, pid, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS) : nullptr;
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == refreshMessage && !pending) {
            pending = true;
            SetTimer(nullptr, 1, 100, nullptr); // One shot; bursts are coalesced, idle has no timer.
        } else if (message.message == WM_TIMER) {
            KillTimer(nullptr, message.wParam);
            pending = false;
            refresh(automation.Get());
        }
    }
    if (hook) UnhookWinEvent(hook);
    automation.Reset();
    CoUninitialize();
    return 0;
}
inline bool start(HWND owner, const std::vector<HWND>& windows) {
    ownerWindow = owner; targets = windows;
    return true;
}
inline bool setEnabled(bool value) {
    enabled = value;
    if (!value) return true;
    if (!thread) {
        queueReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!queueReady) return false;
        thread = CreateThread(nullptr, 0, run, nullptr, 0, &threadId);
        const bool ready = thread && WaitForSingleObject(queueReady, 2000) == WAIT_OBJECT_0;
        if (!ready) return false;
        CloseHandle(queueReady); queueReady = nullptr;
    }
    report = true;
    retries = 0;
    return PostThreadMessageW(threadId, refreshMessage, 0, 0) != 0;
}
inline void stop() {
    enabled = false;
    if (thread) {
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(thread, 2000); // A stuck UIA provider must not block taskbar recovery.
        CloseHandle(thread); thread = nullptr;
    }
}
}
