#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>

namespace umbrella::platform {
struct ThumbnailApi {
    decltype(&DwmRegisterThumbnail) registerThumbnail=&DwmRegisterThumbnail;
    decltype(&DwmQueryThumbnailSourceSize) querySize=&DwmQueryThumbnailSourceSize;
    decltype(&DwmUpdateThumbnailProperties) update=&DwmUpdateThumbnailProperties;
    decltype(&DwmUnregisterThumbnail) unregisterThumbnail=&DwmUnregisterThumbnail;
};
// Borrowed source/destination HWNDs; this object owns only its DWM relationship.
class DockThumbnail final {
public:
    explicit DockThumbnail(ThumbnailApi api={}) : api_(api) {}
    ~DockThumbnail() { reset(); }
    DockThumbnail(const DockThumbnail&)=delete;
    DockThumbnail& operator=(const DockThumbnail&)=delete;
    explicit operator bool() const { return handle_!=nullptr; }
    void reset() {
        if(handle_) api_.unregisterThumbnail(handle_);
        handle_=nullptr;source_=nullptr;destination_=nullptr;
    }
    bool show(HWND destination,HWND source,RECT area) {
        if(!destination || !source || area.right<=area.left || area.bottom<=area.top) { reset(); return false; }
        if(source_!=source || destination_!=destination) reset();
        if(!handle_) {
            if(FAILED(api_.registerThumbnail(destination,source,&handle_))) { handle_=nullptr; return false; }
            source_=source;destination_=destination;
        }
        SIZE size{};
        if(FAILED(api_.querySize(handle_,&size)) || size.cx<=0 || size.cy<=0) { reset(); return false; }
        const double ratio=std::min((area.right-area.left)/double(size.cx),(area.bottom-area.top)/double(size.cy));
        const int width=std::max(1,static_cast<int>(std::lround(size.cx*ratio)));
        const int height=std::max(1,static_cast<int>(std::lround(size.cy*ratio)));
        const int x=area.left+(area.right-area.left-width)/2,y=area.top+(area.bottom-area.top-height)/2;
        DWM_THUMBNAIL_PROPERTIES properties{};
        properties.dwFlags=DWM_TNP_RECTDESTINATION|DWM_TNP_VISIBLE|DWM_TNP_OPACITY|DWM_TNP_SOURCECLIENTAREAONLY;
        properties.rcDestination={x,y,x+width,y+height};properties.opacity=255;properties.fVisible=TRUE;properties.fSourceClientAreaOnly=FALSE;
        if(FAILED(api_.update(handle_,&properties))) { reset(); return false; }
        return true;
    }
private:
    ThumbnailApi api_;
    HTHUMBNAIL handle_=nullptr;
    HWND source_=nullptr,destination_=nullptr;
};
}
