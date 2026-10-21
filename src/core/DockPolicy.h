#pragma once
#include <algorithm>
#include <cmath>

namespace umbrella::dock {
enum class Edge { bottom, top, left, right };
struct Rect { int x, y, width, height; };
struct PreviewLayout {
    Rect bounds;
    int count, columns, cellWidth, cellHeight, header;
    Rect cell(int index) const {
        return {(index%columns)*cellWidth,header+(index/columns)*cellHeight,cellWidth,cellHeight};
    }
};
inline PreviewLayout previewLayout(Rect work, Rect anchor, Edge edge, int count, double dpiScale) {
    count=std::clamp(count,1,8);
    const int header=std::clamp(static_cast<int>(std::lround(30*dpiScale)),0,std::max(0,work.height-1));
    const int preferred=std::max(1,static_cast<int>(std::lround(232*dpiScale)));
    int columns=std::clamp(work.width/preferred,1,std::min(count,4));
    count=std::min(count,columns*std::max(1,work.height-header));
    columns=std::min(columns,count);
    const int rows=(count+columns-1)/columns;
    const int cellWidth=std::max(1,std::min(preferred,work.width/columns));
    const int cellHeight=std::max(1,std::min(static_cast<int>(std::lround(170*dpiScale)),(work.height-header)/rows));
    const int width=columns*cellWidth, height=header+rows*cellHeight;
    const int gap=static_cast<int>(std::lround(8*dpiScale));
    int x=anchor.x+(anchor.width-width)/2, y=anchor.y-height-gap;
    if (edge==Edge::top) y=anchor.y+anchor.height+gap;
    if (edge==Edge::left || edge==Edge::right) {
        x=edge==Edge::left ? anchor.x+anchor.width+gap : anchor.x-width-gap;
        y=anchor.y+(anchor.height-height)/2;
    }
    return {{std::clamp(x,work.x,work.x+std::max(0,work.width-width)),
        std::clamp(y,work.y,work.y+std::max(0,work.height-height)),width,height},count,columns,cellWidth,cellHeight,header};
}
inline int previewHitTest(const PreviewLayout& layout,int x,int y) {
    if(x<0 || y<layout.header || x>=layout.bounds.width || y>=layout.bounds.height || layout.cellWidth<=0 || layout.cellHeight<=0) return -1;
    const int index=(y-layout.header)/layout.cellHeight*layout.columns+x/layout.cellWidth;
    return index<layout.count ? index : -1;
}
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
