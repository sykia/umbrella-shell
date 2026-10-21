#include "platform/windows/DockThumbnail.h"
#include <iostream>
#include <cstdlib>
#include <set>
using namespace umbrella::platform;
void require(bool value,const char* message) { if(!value) { std::cerr<<message<<'\n';std::exit(1); } }
namespace {
int registrations=0,updates=0,releases=0;
std::set<HTHUMBNAIL> live;
bool registrationFails=false,queryFails=false,updateFails=false;
SIZE sourceSize{1280,720};
DWM_THUMBNAIL_PROPERTIES last{};
HRESULT WINAPI reg(HWND,HWND,PHTHUMBNAIL handle) {
    if(registrationFails) return E_ACCESSDENIED;
    *handle=reinterpret_cast<HTHUMBNAIL>(static_cast<INT_PTR>(++registrations));live.insert(*handle);return S_OK;
}
HRESULT WINAPI query(HTHUMBNAIL handle,PSIZE size) {
    require(live.contains(handle),"Query used an unregistered thumbnail");
    if(queryFails) return E_FAIL;
    *size=sourceSize;return S_OK;
}
HRESULT WINAPI update(HTHUMBNAIL handle,const DWM_THUMBNAIL_PROPERTIES* properties) {
    require(live.contains(handle),"Update used an unregistered thumbnail");
    ++updates;last=*properties;return updateFails ? E_FAIL : S_OK;
}
HRESULT WINAPI release(HTHUMBNAIL handle) { require(live.erase(handle)==1,"Thumbnail released twice");++releases;return S_OK; }
}
int main() {
    const ThumbnailApi fake{reg,query,update,release};
    const auto destination=reinterpret_cast<HWND>(1), source=reinterpret_cast<HWND>(2), other=reinterpret_cast<HWND>(3);
    {
        DockThumbnail thumbnail(fake);
        require(thumbnail.show(destination,source,{0,0,200,200}),"Thumbnail initialization");
        require(last.fVisible && last.opacity==255 && !last.fSourceClientAreaOnly,"Thumbnail visibility flags");
        require(last.rcDestination.left>=0 && last.rcDestination.top>=0 && last.rcDestination.right<=200 && last.rcDestination.bottom<=200,"Thumbnail exceeds destination");
        const auto& r=last.rcDestination;
        require(std::abs((r.right-r.left)/double(r.bottom-r.top)-1280./720)<0.03,"Thumbnail aspect ratio");
        require(thumbnail.show(destination,source,{5,5,150,90}) && registrations==1,"Unnecessary DWM registration on resize");
        require(thumbnail.show(destination,other,{0,0,200,200}) && releases==1 && live.size()==1,"Source replacement leaked relationship");
        updateFails=true; require(!thumbnail.show(destination,other,{0,0,200,200}) && live.empty(),"Failed update retained resource");updateFails=false;
        queryFails=true;require(!thumbnail.show(destination,source,{0,0,200,200}) && live.empty(),"Failed query retained resource");queryFails=false;
        sourceSize={0,720};require(!thumbnail.show(destination,source,{0,0,200,200}) && live.empty(),"Invalid source size retained resource");sourceSize={1280,720};
        registrationFails=true;const int oldUpdates=updates;require(!thumbnail.show(destination,source,{0,0,200,200}) && updates==oldUpdates && live.empty(),"Failed registration proceeded to update");registrationFails=false;
        require(thumbnail.show(destination,source,{0,0,1,1}),"Small destination");
        require(!thumbnail.show(destination,source,{0,0,0,1}) && live.empty(),"Empty destination retained resource");
        thumbnail.reset();thumbnail.reset();
        require(thumbnail.show(destination,source,{0,0,200,200}),"Recreate after reset");
    }
    require(live.empty() && registrations==releases,"Destructor leaked DWM relationship");
    std::cout<<"Dock DWM ownership/error-path fixtures passed (mock API, no desktop windows)\n";
}
