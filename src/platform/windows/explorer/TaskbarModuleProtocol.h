#pragma once
#include <Windows.h>
#include <cstdint>

namespace umbrella::explorer {
inline constexpr wchar_t attachMessage[] = L"Umbrella.Taskbar.Attach.3";
inline constexpr wchar_t restoreMessage[] = L"Umbrella.Taskbar.Restore.3";
inline constexpr wchar_t suspendVisualsMessage[] = L"Umbrella.Taskbar.SuspendVisuals.1";
inline constexpr wchar_t ownerClass[] = L"Umbrella.Taskbar.ModuleHost.3";
inline constexpr ULONG_PTR protocol = 0x554d4203;
struct Configuration {
    std::uint32_t size = sizeof(Configuration);
    std::uint32_t version = 3;
    std::uint32_t widthPercent = 100;
    std::uint32_t enabled = 0;
    std::uint32_t edge = 3;
    std::uint32_t thickness = 48;
    std::uint32_t islands = 0;
    std::uint32_t detached = 0;
};
inline constexpr ULONG_PTR islandsProtocol = protocol + 1;
inline constexpr unsigned maxIslands = 128;
struct IslandMask {
    RECT bounds{}; // Screen coordinates from the same UI Automation snapshot.
    std::uint32_t count{};
    RECT rectangles[maxIslands]{}; // Window-relative physical pixels.
};
static_assert(sizeof(Configuration) == 32);
}
