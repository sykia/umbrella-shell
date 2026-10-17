#include "ExplorerRecovery.h"
#include <windows.h>
#include <string>
#include <vector>

namespace umbrella::platform {
std::error_code openExplorer() {
    std::vector<wchar_t> buffer(32768);
    const UINT length = GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
    if (length == 0) return {static_cast<int>(GetLastError()), std::system_category()};
    if (length >= buffer.size()) return std::make_error_code(std::errc::filename_too_long);
    const std::wstring executable = std::wstring(buffer.data(), length) + L"\\explorer.exe";
    std::wstring command = L"\"" + executable + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &startup, &process)) {
        return {static_cast<int>(GetLastError()), std::system_category()};
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return {};
}
}
