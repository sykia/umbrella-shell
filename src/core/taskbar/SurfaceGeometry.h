#pragma once
#include "Placement.h"

namespace umbrella::taskbar {
struct SurfaceSpec {
    bool enabled=false;
    unsigned edge=3,widthPercent=100,thickness=48;
    double inset=0;
};
struct SurfaceMeasurement {
    Rectangle panel,monitor;
    double dpi=0,rootWidth=0,rootHeight=0,rasterScale=0,rootX=0,rootY=0;
    bool autoHide=false;
};
// Physical rectangles share the desktop coordinate space; viewport is root DIP.
// A known appearance policy is necessary but does not prove the measured XAML
// surface follows it. Reject stale/mismatched snapshots before arranging slots.
inline bool surfaceViewport(const SurfaceSpec& spec,const SurfaceMeasurement& m,Rectangle& viewport) noexcept {
    if(!spec.enabled || (spec.edge!=1 && spec.edge!=3) || spec.widthPercent<40 || spec.widthPercent>100 ||
        spec.thickness<48 || spec.thickness>96 || !std::isfinite(spec.inset) || spec.inset<0 || spec.inset>32 ||
        !valid(m.panel) || !valid(m.monitor) || !std::isfinite(m.dpi) || m.dpi<96 || m.dpi>384 || m.autoHide ||
        !std::isfinite(m.rootWidth) || !std::isfinite(m.rootHeight) || !std::isfinite(m.rasterScale) ||
        !std::isfinite(m.rootX) || !std::isfinite(m.rootY))return false;
    const auto scale=m.dpi/96;
    if(std::abs(m.rasterScale-scale)>.0001 || std::abs(m.rootX)>.01 || std::abs(m.rootY)>.01 ||
        std::abs(m.rootWidth*scale-m.panel.width)>1 || std::abs(m.rootHeight*scale-m.panel.height)>1 ||
        std::abs(m.panel.height-spec.thickness*scale)>1 ||
        std::abs(m.panel.width-m.monitor.width*spec.widthPercent/100)>1 ||
        std::abs(m.panel.x+m.panel.width/2-m.monitor.x-m.monitor.width/2)>1 ||
        std::abs((spec.edge==3?m.panel.y+m.panel.height:m.panel.y)-
            (spec.edge==3?m.monitor.y+m.monitor.height:m.monitor.y))>1)return false;
    if(!inside({m.panel.x-m.monitor.x,m.panel.y-m.monitor.y,m.panel.width,m.panel.height},m.monitor.width,m.monitor.height))return false;
    viewport={spec.inset,0,m.rootWidth-2*spec.inset,m.rootHeight};
    return inside(viewport,m.rootWidth,m.rootHeight);
}
}
