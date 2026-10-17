#include "PanelActions.h"
#include <windows.h>
#include <shellapi.h>
#include <array>
namespace umbrella::platform {
namespace {
bool shortcut(WORD key, bool control) {
    // Do not release modifiers physically held by the user.
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN) | GetAsyncKeyState(VK_CONTROL) |
         GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_SHIFT)) & 0x8000) return false;
    std::array<INPUT, 6> inputs{}; UINT count = 0;
    const auto add = [&](WORD code, DWORD flags) { auto& input = inputs[count++]; input.type = INPUT_KEYBOARD; input.ki.wVk = code; input.ki.dwFlags = flags; };
    add(VK_LWIN, 0); if (control) add(VK_CONTROL, 0);
    const DWORD extended = key == VK_TAB ? 0 : KEYEVENTF_EXTENDEDKEY;
    add(key, extended); add(key, extended | KEYEVENTF_KEYUP);
    if (control) add(VK_CONTROL, KEYEVENTF_KEYUP); add(VK_LWIN, KEYEVENTF_KEYUP);
    if (SendInput(count, inputs.data(), sizeof(INPUT)) == count) return true;
    std::array<INPUT, 3> release{};
    release[0].type = release[1].type = release[2].type = INPUT_KEYBOARD;
    release[0].ki.wVk = key; release[0].ki.dwFlags = extended | KEYEVENTF_KEYUP;
    release[1].ki.wVk = VK_CONTROL; release[1].ki.dwFlags = KEYEVENTF_KEYUP;
    release[2].ki.wVk = VK_LWIN; release[2].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(static_cast<UINT>(release.size()), release.data(), sizeof(INPUT));
    return false;
}
}
PanelActions::PanelActions(QObject* parent) : QObject(parent) {
    timer_.setInterval(750); timer_.setTimerType(Qt::CoarseTimer);
    connect(&timer_, &QTimer::timeout, this, &PanelActions::refresh);
}
void PanelActions::setEnabled(bool enabled) {
    if (enabled) { refresh(); timer_.start(); } else timer_.stop();
}
void PanelActions::refresh() {
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0; const DWORD thread = GetWindowThreadProcessId(foreground, &process);
    if (foreground && thread && process != GetCurrentProcessId()) target_ = reinterpret_cast<quintptr>(foreground);
    const HWND target = reinterpret_cast<HWND>(target_);
    const DWORD targetThread = target && IsWindow(target) ? GetWindowThreadProcessId(target, nullptr) : 0;
    if (!targetThread) return;
    const LANGID language = LOWORD(reinterpret_cast<ULONG_PTR>(GetKeyboardLayout(targetThread)));
    wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
    QString value = "—";
    if (GetLocaleInfoW(MAKELCID(language, SORT_DEFAULT), LOCALE_SISO639LANGNAME, name, LOCALE_NAME_MAX_LENGTH))
        value = QString::fromWCharArray(name).toUpper();
    if (layout_ != value) { layout_ = value; emit layoutChanged(); }
}
bool PanelActions::cycleLayout() {
    refresh(); const HWND target = reinterpret_cast<HWND>(target_);
    if (!target || !IsWindow(target)) return false;
    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count < 2 || count > 64) return false;
    std::array<HKL, 64> layouts{};
    const int loaded = GetKeyboardLayoutList(count, layouts.data());
    if (loaded < 2) return false;
    const HKL current = GetKeyboardLayout(GetWindowThreadProcessId(target, nullptr));
    int next = 0;
    for (int i = 0; i < loaded; ++i) if (layouts[i] == current) { next = (i + 1) % loaded; break; }
    return PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(layouts[next])) != FALSE;
}
bool PanelActions::openSound() { return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL)) > 32; }
bool PanelActions::moveDesktop(int direction) { return (direction == -1 || direction == 1) && shortcut(direction < 0 ? VK_LEFT : VK_RIGHT, true); }
bool PanelActions::showDesktops() { return shortcut(VK_TAB, false); }
}
