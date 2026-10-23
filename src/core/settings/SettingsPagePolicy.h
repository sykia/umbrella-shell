#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace umbrella::settings {
// Physical screen coordinates from UIA, never inferred from window size or DPI.
struct Rect {
    int left{}, top{}, right{}, bottom{};
    bool valid() const { return right > left && bottom > top; }
    bool contains(Rect r) const { return valid() && r.valid() && r.left >= left && r.top >= top && r.right <= right && r.bottom <= bottom; }
    bool overlaps(Rect r) const { return left < r.right && r.left < right && top < r.bottom && r.top < bottom; }
};
struct Node {
    int parent = -1;
    std::wstring id;
    Rect bounds;
    bool visible = false, title = false, scroll = false, navigation = false;
};
inline int anchorKind(const std::wstring& id) {
    // Exact IDs observed in the real RU Settings control tree. A wrapper and
    // its child control count as ONE setting, never two independent anchors.
    if (id == L"SystemSettings_DesktopTaskbar_Search_EntityItem" || id == L"SystemSettings_DesktopTaskbar_Search_ComboBox") return 1;
    if (id == L"SystemSettings_DesktopTaskbar_TaskView_EntityItem" || id == L"SystemSettings_DesktopTaskbar_TaskView_ToggleSwitch") return 2;
    if (id == L"SystemSettings_DesktopTaskbar_InkWorkspace_EntityItem" || id == L"SystemSettings_DesktopTaskbar_InkWorkspace_ToggleSwitch") return 4;
    if (id == L"SystemSettings_Taskbar_Autohide_CheckBox") return 8;
    if (id == L"SystemSettings_Taskbar_Badging_CheckBox") return 16;
    if (id == L"SystemSettings_DesktopTaskbar_Al_ComboBox") return 32;
    if (id == L"SystemSettings_Taskbar_MultiMon_CheckBox") return 64;
    if (id == L"SystemSettings_DesktopTaskbar_GroupingMode_ComboBox") return 128;
    if (id == L"SystemSettings_DesktopTaskbar_IconSizePreference_ComboBox") return 256;
    return 0;
}
// Unknown builds/languages and incomplete snapshots deliberately leave Settings usable.
inline std::optional<Rect> contentBounds(const std::vector<Node>& nodes, Rect client, bool complete) {
    if (!complete || nodes.empty() || nodes.size() > 256 || !client.valid()) return {};
    for (size_t i = 0; i < nodes.size(); ++i)
        if (nodes[i].parent < -1 || nodes[i].parent >= static_cast<int>(i)) return {};
    int title = -1, kinds = 0;
    std::vector<int> anchors;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        const auto& node = nodes[i];
        if (!node.visible || !client.contains(node.bounds)) continue;
        if (node.title) { if (title != -1) return {}; title = i; }
        if (const auto kind = anchorKind(node.id)) { kinds |= kind; anchors.push_back(i); }
    }
    if (title < 0 || anchors.size() < 2 || (kinds & (kinds - 1)) == 0) return {};
    const auto descends = [&](int child, int ancestor) {
        for (int index = child; index >= 0; index = nodes[index].parent) if (index == ancestor) return true;
        return false;
    };
    // A real UIA scroll viewport is required; no hard-coded page offsets.
    for (int candidate = nodes[anchors.front()].parent; candidate >= 0; candidate = nodes[candidate].parent) {
        const auto& node = nodes[candidate];
        if (!node.visible || !node.scroll || !client.contains(node.bounds)) continue;
        if (!std::all_of(anchors.begin(), anchors.end(), [&](int a) { return descends(a, candidate); })) continue;
        Rect result = node.bounds;
        result.top = std::max(result.top, nodes[title].bounds.bottom);
        if (!result.valid() || result.overlaps(nodes[title].bounds)) return {};
        for (const auto& other : nodes)
            if (other.visible && other.navigation && result.overlaps(other.bounds)) return {};
        for (const auto anchor : anchors) if (!result.contains(nodes[anchor].bounds)) return {};
        return result;
    }
    return {};
}
}
