#pragma once
#include <Windows.h>
#include <vector>

namespace umbrella::platform {
// Explorer's primary panel is not always included by EnumWindows on Windows 11.
// Enumerate the two exact shell classes, bounded; callers still verify ownership.
inline std::vector<HWND> taskbarWindows() {
    std::vector<HWND> result;
    for (const auto name : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd"}) {
        HWND previous{};
        while ((previous = FindWindowExW(nullptr, previous, name, nullptr)) != nullptr) {
            result.push_back(previous);
            if (result.size() > 16) return result;
        }
    }
    return result;
}
}
