#include "platform/windows/DesktopBar.h"
#include <iostream>
#include <memory>
using namespace umbrella::platform;
namespace {
struct Close { void operator()(void* handle) const { if (handle) CloseHandle(handle); } };
using Handle = std::unique_ptr<void, Close>;
int calls = 0;
bool expected = false, succeeds = true, matched = true;
bool restoreProbe(bool value) { ++calls; matched = matched && value == expected; return succeeds; }
}
int main(int argc, char**) {
    if (argc == 2) return 0; // Short-lived synthetic parent. Never touches Explorer.
    Handle instance(CreateMutexW(nullptr, FALSE, L"Local\\UmbrellaShell.DesktopBar"));
    if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) return 1;
    const std::wstring token = L"Local\\UmbrellaShell.Test." + std::to_wstring(GetCurrentProcessId());
    const auto readyName = token + L".ready", stopName = token + L".stop";
    Handle ready(CreateEventW(nullptr, TRUE, FALSE, readyName.c_str()));
    Handle stop(CreateEventW(nullptr, TRUE, TRUE, stopName.c_str()));
    if (!ready || !stop) return 1;
    // The signal path must restore both possible original modes, report readiness,
    // and propagate restoration errors, without mutating the real desktop.
    for (bool original : {false, true}) {
        expected = original; succeeds = true;
        if (guardTaskbar(GetCurrentProcessId(), original ? ABS_AUTOHIDE : 0,
                         readyName, stopName, restoreProbe) != 0 ||
            WaitForSingleObject(ready.get(), 0) != WAIT_OBJECT_0 || !matched) return 1;
        ResetEvent(ready.get());
    }
    succeeds = false;
    if (guardTaskbar(GetCurrentProcessId(), ABS_AUTOHIDE, readyName, stopName, restoreProbe) == 0) return 1;
    // Parent-death path: keep a handle to the exited process so its PID cannot
    // be reused before guard opens it. The stop event is deliberately unsignalled.
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, 32768)) return 1;
    std::wstring command = L"\"" + std::wstring(path) + L"\" --child";
    STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
    if (!CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &startup, &process)) return 1;
    Handle child(process.hProcess), thread(process.hThread);
    if (WaitForSingleObject(child.get(), 5000) != WAIT_OBJECT_0) return 1;
    ResetEvent(stop.get()); succeeds = true; expected = false;
    if (guardTaskbar(process.dwProcessId, 0, readyName, stopName, restoreProbe) != 0 || !matched) return 1;
    const int before = calls;
    if (guardTaskbar(GetCurrentProcessId(), 0, token + L".missing", stopName, restoreProbe) == 0 || calls != before) return 1;
    std::cout << "Guard stop, parent death, original modes and failure propagation passed\n";
    return 0;
}
