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

// Opt-in developer experiment; never included in the persistent configuration.
inline constexpr ULONG_PTR editProtocol = protocol + 2;
inline constexpr ULONG_PTR editReportProtocol = protocol + 3;
enum class EditAction : std::uint32_t { Inspect, Begin, Preview, Reset, Status, Place, AfterWidgets };
struct EditRequest {
    std::uint32_t size = sizeof(EditRequest);
    std::uint32_t version = 1;
    EditAction action = EditAction::Inspect;
    double position = 0; // Absolute X within the existing parent, DIP; Place only.
};
struct EditAncestor {
    wchar_t type[128]{};
    wchar_t name[80]{};
    double width{}, height{};
};
struct EditReport {
    std::uint32_t size = sizeof(EditReport);
    std::uint32_t version = 1;
    HRESULT status = E_FAIL;
    std::uint32_t editing{}, dragging{}, moves{}, clicks{}, focusEvents{};
    std::uint32_t hitTestVisible{}, rootBackground{};
    double offset{}, buttonX{}, buttonY{}, buttonWidth{}, parentWidth{};
    wchar_t blockerType[128]{};
    double blockerX{}, blockerWidth{};
    std::uint32_t ancestors{};
    EditAncestor chain[8]{}; // Interactive button first, then immediate visual parents.
};
// Separate, versioned read-only diagnostic protocol; legacy edit ABI is intact.
inline constexpr ULONG_PTR componentInspectProtocol = protocol + 4;
inline constexpr ULONG_PTR componentSnapshotProtocol = protocol + 5;
inline constexpr unsigned maxComponentSnapshot = 48;
struct ComponentSnapshotEntry {
    wchar_t id[24]{}, type[96]{}, parentType[96]{}, parentName[80]{};
    double x{}, y{}, width{}, height{};
    std::uint32_t visible{}, hitTestVisible{};
};
struct ComponentSnapshot {
    std::uint32_t size = sizeof(ComponentSnapshot), version = 1;
    HRESULT status = E_FAIL;
    std::uint32_t count{}, visited{};
    ComponentSnapshotEntry entries[maxComponentSnapshot]{};
};
static_assert(sizeof(ComponentSnapshot) < 65536);
}
