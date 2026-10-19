#include "SettingsWindowControl.h"
#include "SettingsWindowMessages.h"
#include <QGuiApplication>
#include <QWindow>
#include <Windows.h>
#include <array>
namespace umbrella::platform {
bool activateExistingSettingsWindow() {
    const HWND window = FindWindowW(nullptr, L"Umbrella — Настройки");
    if (!window) return false;
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::array<wchar_t, 32768> ours{}, theirs{};
    const DWORD length = GetModuleFileNameW(nullptr, ours.data(), static_cast<DWORD>(ours.size()));
    DWORD size = static_cast<DWORD>(theirs.size());
    const bool same = length && length < ours.size() && QueryFullProcessImageNameW(process, 0, theirs.data(), &size) &&
        !_wcsicmp(ours.data(), theirs.data());
    CloseHandle(process);
    if (!same) return false;
    AllowSetForegroundWindow(pid);
    return PostMessageW(window, RegisterWindowMessageW(settings_messages::show), 0, 0) != FALSE;
}
SettingsWindowControl::SettingsWindowControl(QGuiApplication& app, QWindow& window) : app_(app), window_(window) {
    app_.installNativeEventFilter(this);
}
SettingsWindowControl::~SettingsWindowControl() { app_.removeNativeEventFilter(this); }
bool SettingsWindowControl::nativeEventFilter(const QByteArray&, void* message, qintptr* result) {
    const auto* native = static_cast<const MSG*>(message);
    if (!native || native->hwnd != reinterpret_cast<HWND>(window_.winId())) return false;
    if (native->message == RegisterWindowMessageW(settings_messages::show)) {
        window_.show(); window_.raise(); window_.requestActivate();
    } else if (native->message == RegisterWindowMessageW(settings_messages::quit)) {
        // QGuiApplication::quit closes even hidden windows first; our Close
        // filter deliberately rejects that. Explicit shutdown must leave the
        // event loop directly so normal stack/RAII restoration can run.
        app_.exit(0);
    } else return false;
    if (result) *result = 0;
    return true;
}
}
