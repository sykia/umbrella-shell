#pragma once
#include <algorithm>
#include <cmath>

namespace umbrella::dock {
enum class Edge { bottom, top, left, right };
struct Rect { int x, y, width, height; };
inline Rect geometry(Rect screen, Edge edge, int length, int thickness, int margin, int alignment) {
    const bool vertical = edge == Edge::left || edge == Edge::right;
    margin = std::clamp(margin, 0, std::max(0, std::min(screen.width, screen.height) / 4));
    length = std::clamp(length, 1, std::max(1, (vertical ? screen.height : screen.width) - 2 * margin));
    thickness = std::clamp(thickness, 1, std::max(1, (vertical ? screen.width : screen.height) - 2 * margin));
    int along = margin;
    const int available = (vertical ? screen.height : screen.width) - length;
    if (alignment == 1) along = available / 2;
    if (alignment == 2) along = available - margin;
    if (vertical) return {edge == Edge::right ? screen.x + screen.width - thickness - margin : screen.x + margin, screen.y + along, thickness, length};
    return {screen.x + along, edge == Edge::bottom ? screen.y + screen.height - thickness - margin : screen.y + margin, length, thickness};
}
inline bool shouldHide(int mode, bool hovered, bool menu, bool overlap, bool fullscreen, bool hideFullscreen) {
    if (menu || hovered) return false;
    return (hideFullscreen && fullscreen) || mode == 1 || (mode == 2 && overlap);
}
inline Rect physicalRect(Rect surface, Rect logicalMonitor, Rect physicalMonitor, double scale) {
    return {physicalMonitor.x+static_cast<int>(std::lround((surface.x-logicalMonitor.x)*scale)),
        physicalMonitor.y+static_cast<int>(std::lround((surface.y-logicalMonitor.y)*scale)),
        static_cast<int>(std::lround(surface.width*scale)),static_cast<int>(std::lround(surface.height*scale))};
}
}
