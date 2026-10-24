#pragma once
#include "TaskbarComponentPlacement.h"
#include "TaskbarSmoothClip.h"
#include <cmath>

namespace umbrella::explorer::placement_surface {
// Shared by UIA masks and native slot transactions; input coordinates are pixels.
inline bool apply(HWND hwnd,smooth_clip::Session& smooth,const IslandMask& mask) noexcept {
    RECT now{};
    if(!mask.count || mask.count>maxIslands || !GetWindowRect(hwnd,&now) || !EqualRect(&now,&mask.bounds))return false;
    HRGN combined=CreateRectRgn(0,0,0,0);
    if(!combined)return false;
    bool valid=true;
    const int gap=MulDiv(2,static_cast<int>(GetDpiForWindow(hwnd)),96);
    std::array<RECT,maxIslands> rectangles{};
    for(unsigned i=0;i<mask.count;++i) {
        auto r=mask.rectangles[i];
        if(r.left<0 || r.top<0 || r.right>now.right-now.left || r.bottom>now.bottom-now.top ||
            r.right<=r.left || r.bottom<=r.top){valid=false;break;}
        const int inset=(std::min)(gap,static_cast<int>((r.right-r.left)/5));
        InflateRect(&r,-inset,-gap);
        if(r.left<0 || r.top<0 || r.right>now.right-now.left || r.bottom>now.bottom-now.top ||
            r.right<=r.left || r.bottom<=r.top){valid=false;break;}
        rectangles[i]=r;
        HRGN part=CreateRectRgn(r.left,r.top,r.right,r.bottom);
        if(!part){valid=false;break;}
        if(CombineRgn(combined,combined,part,RGN_OR)==ERROR)valid=false;
        DeleteObject(part);if(!valid)break;
    }
    if(!valid || !smooth.apply(hwnd,rectangles.data(),mask.count,12.0f) || !SetWindowRgn(hwnd,combined,TRUE)) {
        DeleteObject(combined);return false;
    }
    return true; // HWND owns combined after SetWindowRgn.
}

class Update final:public component_layout::ClipUpdate {
    HWND hwnd_{};
    smooth_clip::Session& smooth_;
    IslandMask& current_;
    bool& applied_;
    IslandMask before_{},after_{};
    bool wasApplied_=false,hadRegion_=false;
    HRGN region_{};
    smooth_clip::Session::Snapshot composition_;
public:
    Update(HWND hwnd,smooth_clip::Session& smooth,IslandMask& current,bool& applied):
        hwnd_(hwnd),smooth_(smooth),current_(current),applied_(applied),before_(current),wasApplied_(applied),composition_(smooth.snapshot()) {}
    ~Update() override {if(region_)DeleteObject(region_);}
    bool captureRegion() noexcept {
        region_=CreateRectRgn(0,0,0,0);if(!region_)return false;
        hadRegion_=GetWindowRgn(hwnd_,region_)!=ERROR;
        const auto bytes=hadRegion_?GetRegionData(region_,0,nullptr):0;
        return !hadRegion_ || (bytes>0 && bytes<=65536);
    }
    bool prepareMask(const IslandMask& mask) noexcept {
        RECT now{};
        if(!mask.count || mask.count>maxIslands || !GetWindowRect(hwnd_,&now) || !EqualRect(&now,&mask.bounds))return false;
        after_=mask;return captureRegion();
    }
    bool prepare(std::span<const taskbar::Rectangle> boxes,double rootWidth,double rootHeight) noexcept {
        if(boxes.empty() || boxes.size()>taskbar::maxReorderItems || !GetWindowRect(hwnd_,&after_.bounds))return false;
        const auto dpi=GetDpiForWindow(hwnd_);if(!dpi)return false;
        const double scale=static_cast<double>(dpi)/96;
        const auto width=after_.bounds.right-after_.bounds.left,height=after_.bounds.bottom-after_.bounds.top;
        // Never map slots through an assumed 48-DIP or full-monitor surface.
        if(!std::isfinite(rootWidth) || !std::isfinite(rootHeight) ||
            std::abs(rootWidth*scale-width)>1 || std::abs(rootHeight*scale-height)>1)return false;
        if(!captureRegion())return false;
        const double padding=6*scale;
        for(const auto box:boxes) {
            if(!taskbar::inside(box,rootWidth,rootHeight))return false;
            after_.rectangles[after_.count++]={
                static_cast<LONG>((std::max)(0.,std::floor(box.x*scale-padding))),
                static_cast<LONG>((std::max)(0.,std::floor(box.y*scale-padding))),
                static_cast<LONG>((std::min)(static_cast<double>(width),std::ceil((box.x+box.width)*scale+padding))),
                static_cast<LONG>((std::min)(static_cast<double>(height),std::ceil((box.y+box.height)*scale+padding)))};
        }
        // Adjacent native controls form one island, rather than a row of
        // overlapping rounded caps. Deliberate larger gaps remain separate.
        bool merged=true;
        while(merged) {
            merged=false;
            for(unsigned i=0;i<after_.count && !merged;++i)for(unsigned j=i+1;j<after_.count;++j) {
                auto& a=after_.rectangles[i];const auto b=after_.rectangles[j];
                if(a.right<b.left || b.right<a.left || a.bottom<=b.top || b.bottom<=a.top)continue;
                a={(std::min)(a.left,b.left),(std::min)(a.top,b.top),(std::max)(a.right,b.right),(std::max)(a.bottom,b.bottom)};
                for(unsigned k=j+1;k<after_.count;++k)after_.rectangles[k-1]=after_.rectangles[k];
                after_.rectangles[--after_.count]={};merged=true;break;
            }
        }
        return true;
    }
    bool apply() noexcept override {
        if(!placement_surface::apply(hwnd_,smooth_,after_))return false;
        current_=after_;applied_=true;return true;
    }
    bool rollback() noexcept override {
        HRGN copy=nullptr;
        if(hadRegion_) {
            copy=CreateRectRgn(0,0,0,0);
            if(!copy || CombineRgn(copy,region_,nullptr,RGN_COPY)==ERROR){if(copy)DeleteObject(copy);return false;}
        }
        const bool composition=smooth_.restore(composition_);
        if(!SetWindowRgn(hwnd_,copy,TRUE)){if(copy)DeleteObject(copy);return false;}
        current_=before_;applied_=wasApplied_;return composition;
    }
};
inline std::unique_ptr<component_layout::ClipUpdate> prepare(HWND hwnd,smooth_clip::Session& smooth,
    IslandMask& current,bool& applied,std::span<const taskbar::Rectangle> boxes,double rootWidth,double rootHeight) {
    auto update=std::make_unique<Update>(hwnd,smooth,current,applied);
    if(!update->prepare(boxes,rootWidth,rootHeight))return {};
    return update;
}
inline bool applyPacket(HWND hwnd,smooth_clip::Session& smooth,IslandMask& current,bool& applied,const IslandMask& mask) noexcept {
    try {
        auto update=std::make_unique<Update>(hwnd,smooth,current,applied);
        if(!update->prepareMask(mask))return false;
        if(update->apply())return true;
        update->rollback();return false;
    }catch(...){return false;}
}
}
