#pragma once
// Explicit, interactive developer test only. Never called by --serve or CTest.
#include <UIAutomation.h>
#include <wrl/client.h>
#include <functional>
#include <cmath>

namespace umbrella::explorer::button_edit_verification {
using Microsoft::WRL::ComPtr;
inline bool moveMouse(LONG x, LONG y) {
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width < 2 || height < 2) return false;
    INPUT input{}; input.type = INPUT_MOUSE;
    input.mi.dx = MulDiv(x - left, 65535, width - 1); input.mi.dy = MulDiv(y - top, 65535, height - 1);
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    return SendInput(1, &input, sizeof(input)) == 1;
}
inline bool mouse(DWORD flags) {
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = flags;
    return SendInput(1, &input, sizeof(input)) == 1;
}
inline void escape() {
    INPUT keys[2]{}; keys[0].type = keys[1].type = INPUT_KEYBOARD;
    keys[0].ki.wVk = keys[1].ki.wVk = VK_ESCAPE; keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, keys, sizeof(INPUT));
}
inline bool targetAt(IUIAutomation* automation, LONG x, LONG y, IUIAutomationElement* button) {
    ComPtr<IUIAutomationElement> hit; ComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(automation->ElementFromPoint({x, y}, &hit)) || FAILED(automation->get_RawViewWalker(&walker))) return false;
    for (unsigned i = 0; hit && i < 10; ++i) {
        BOOL same{};
        if (SUCCEEDED(automation->CompareElements(hit.Get(), button, &same)) && same) return true;
        ComPtr<IUIAutomationElement> parent;
        if (FAILED(walker->GetParentElement(hit.Get(), &parent))) return false;
        hit = parent;
    }
    return false;
}
inline bool run(HWND taskbar, const std::function<bool(EditAction, EditReport&)>& command, bool leaveMoved = false, const std::function<bool(int)>& placement = {}) {
    if ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) return false;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) return false;
    struct ComExit { ~ComExit() { CoUninitialize(); } } comExit;
    POINT cursor{}; GetCursorPos(&cursor);
    struct CursorExit {
        POINT p; HWND foreground;
        ~CursorExit() {
            mouse(MOUSEEVENTF_RIGHTUP); mouse(MOUSEEVENTF_LEFTUP); moveMouse(p.x, p.y);
            if (IsWindow(foreground)) SetForegroundWindow(foreground);
        }
    } cursorExit{cursor, GetForegroundWindow()};
    ComPtr<IUIAutomation> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation)))) return false;
    ComPtr<IUIAutomation2> bounded;
    if (SUCCEEDED(automation.As(&bounded))) { bounded->put_ConnectionTimeout(1000); bounded->put_TransactionTimeout(1500); }
    ComPtr<IUIAutomationElement> root, button;
    ComPtr<IUIAutomationCondition> condition;
    VARIANT id{}; id.vt = VT_BSTR; id.bstrVal = SysAllocString(L"TaskViewButton");
    const HRESULT created = automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, id, &condition);
    VariantClear(&id);
    if (FAILED(created) || FAILED(automation->ElementFromHandle(taskbar, &root)) ||
        FAILED(root->FindFirst(TreeScope_Descendants, condition.Get(), &button)) || !button) return false;
    RECT before{}, panel{}; EditReport report;
    if (FAILED(button->get_CurrentBoundingRectangle(&before)) || !GetWindowRect(taskbar, &panel) || !command(EditAction::Status, report)) return false;
    // Confirm native hit testing before switching back to editor-owned input.
    if (!command(EditAction::Preview, report)) return false;
    // A foreground fullscreen app can cover an otherwise standard taskbar.
    // Request normal taskbar focus; never click through a covering window.
    SetForegroundWindow(taskbar);
    button->SetFocus();
    Sleep(150);
    const LONG width = before.right - before.left, y = (before.top + before.bottom) / 2;
    const LONG start = (before.left + before.right) / 2;
    const bool hit = targetAt(automation.Get(), start, y, button.Get());
    const HWND hitRoot = GetAncestor(WindowFromPoint({start, y}), GA_ROOT);
    wchar_t hitClass[128]{}; GetClassNameW(hitRoot, hitClass, 128);
    std::printf("verify: source=(%ld,%ld,%ld,%ld) uiaHit=%u rootClass=%ls rootMatches=%u\n", before.left, before.top,
        before.right, before.bottom, hit ? 1u : 0u, hitClass, hitRoot == taskbar ? 1u : 0u); std::fflush(stdout);
    if (width <= 0 || !hit || hitRoot != taskbar) return false;
    if (!placement && !command(EditAction::Begin, report)) return false;
    // Find a free destination using only current button rectangles, not saved inventory coordinates.
    VARIANT type{}; type.vt = VT_I4; type.lVal = UIA_ButtonControlTypeId;
    ComPtr<IUIAutomationCondition> buttonsCondition;
    ComPtr<IUIAutomationElementArray> buttons;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, type, &buttonsCondition)) ||
        FAILED(root->FindAll(TreeScope_Descendants, buttonsCondition.Get(), &buttons))) return false;
    int count{}; buttons->get_Length(&count);
    if (count < 1 || count > 128) return false;
    LONG destination{};
    for (LONG x = panel.left + 250; x + width < panel.left + report.parentWidth && x < before.left - 100; x += width + 12) {
        bool free = true;
        for (int i = 0; i < count; ++i) {
            ComPtr<IUIAutomationElement> other; RECT r{}; BOOL offscreen{};
            if (FAILED(buttons->GetElement(i, &other)) || FAILED(other->get_CurrentBoundingRectangle(&r)) || FAILED(other->get_CurrentIsOffscreen(&offscreen))) return false;
            if (!offscreen && r.right > r.left && x < r.right + 8 && x + width > r.left - 8 && before.top < r.bottom && before.bottom > r.top) { free = false; break; }
        }
        if (free) { destination = x + width / 2; break; }
    }
    if (!destination) { std::puts("verify: no unoccupied destination"); return false; }
    if (placement) {
        if (!placement(destination - width / 2 - panel.left)) return false;
        Sleep(350);
        if (!command(EditAction::Status, report) || report.editing || report.dragging || !report.hitTestVisible) return false;
        std::puts("verify: GUI placement; native hit test remains enabled, no taskbar drag");
    } else {
    std::printf("verify: physical left-drag (%ld,%ld) -> (%ld,%ld)\n", start, y, destination, y); std::fflush(stdout);
    if (!moveMouse(start, y)) return false;
    Sleep(150);
    if (!mouse(MOUSEEVENTF_LEFTDOWN)) return false;
    for (int i = 1; i <= 20; ++i) { Sleep(25); if (!moveMouse(start + MulDiv(destination - start, i, 20), y)) return false; }
    command(EditAction::Status, report);
    std::printf("verify: captured drag offset=%.1f\n", report.offset);
    mouse(MOUSEEVENTF_LEFTUP); Sleep(350);
    }
    RECT moved{};
    bool ok = SUCCEEDED(button->get_CurrentBoundingRectangle(&moved)) && command(EditAction::Status, report) &&
        report.moves > 0 && std::abs(moved.left - before.left) > 80 &&
        std::abs((moved.left + moved.right) / 2 - destination) <= 3 && report.clicks == 0;
    if (ok && leaveMoved) {
        std::printf("verify: moved=1 beforeX=%ld afterX=%ld realMoves=%u\n", before.left, moved.left, report.moves);
        return true; // Leave editor input properties armed too, to exercise their recovery.
    }
    ok = command(EditAction::Preview, report) && ok;
    ok = targetAt(automation.Get(), destination, y, button.Get()) && !targetAt(automation.Get(), start, y, button.Get()) && ok;
    std::printf("verify: moved=%u beforeX=%ld afterX=%ld realMoves=%u\n", ok ? 1u : 0u, before.left, moved.left, report.moves);
    if (ok) {
        ok = command(EditAction::Preview, report);
        const unsigned clicks = report.clicks;
        if (ok) {
            moveMouse(destination, y); mouse(MOUSEEVENTF_LEFTDOWN); Sleep(60); mouse(MOUSEEVENTF_LEFTUP); Sleep(700);
            wchar_t foregroundClass[128]{};
            const HWND foreground = GetForegroundWindow(); GetClassNameW(foreground, foregroundClass, 128);
            const bool clicked = command(EditAction::Status, report) && report.clicks > clicks;
            std::printf("verify: nativeClick=%u foregroundClass=%ls\n", clicked ? 1u : 0u, foregroundClass);
            // Report actual foreground shell surface. No title or user content is recorded.
            const bool taskView = clicked && (!wcscmp(foregroundClass, L"XamlExplorerHostIslandWindow") || !wcscmp(foregroundClass, L"MultitaskingViewFrame"));
            if (taskView) { escape(); Sleep(500); SetForegroundWindow(taskbar); }
            const HRESULT focused = taskView ? button->SetFocus() : E_ABORT; BOOL hasFocus{};
            if (taskView) for (unsigned i = 0; i < 10; ++i) {
                Sleep(50);
                if (SUCCEEDED(button->get_CurrentHasKeyboardFocus(&hasFocus)) && hasFocus) break;
            }
            std::printf("verify: shellSurface=%u focusHRESULT=%08lx hasFocus=%u\n", taskView ? 1u : 0u, static_cast<unsigned long>(focused), hasFocus ? 1u : 0u);
            ok = clicked && taskView && SUCCEEDED(focused) && hasFocus;
        }
    }
    const bool reset = command(EditAction::Reset, report);
    RECT after{};
    const bool restored = reset && SUCCEEDED(button->get_CurrentBoundingRectangle(&after)) && EqualRect(&before, &after);
    std::printf("verify: originalRectangleRestored=%u\n", restored ? 1u : 0u); std::fflush(stdout);
    return ok && restored;
}
}
