#pragma once
#include <string_view>
namespace umbrella::core {
constexpr bool validPanelEdge(std::string_view value) {
    return value == "top" || value == "bottom" || value == "left" || value == "right";
}
constexpr bool validModule(std::string_view value) {
    return value == "workspaces" || value == "keyboard" || value == "volume";
}
constexpr bool validZone(int zone) { return zone >= -1 && zone <= 2; }
constexpr bool validWorkspaceCount(int count) { return count >= 1 && count <= 10; }
}
