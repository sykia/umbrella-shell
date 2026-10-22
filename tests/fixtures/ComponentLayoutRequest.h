#pragma once
#include <Windows.h>
#include <algorithm>
#include <iterator>
#include "core/taskbar/ComponentRegistry.h"

// Isolated fixture data only. No Explorer IPC command or production adapter.
namespace umbrella::explorer {
inline constexpr unsigned maxLayoutGroups=32;
struct ComponentLayoutAssignment {wchar_t id[24]{};std::uint32_t zone{};};
struct ComponentLayoutRequest {
    std::uint32_t size=sizeof(ComponentLayoutRequest),version=1,count{},gap=8,flags{};
    ComponentLayoutAssignment entries[maxLayoutGroups]{};
};
// Empty request means restore the original layout, never hide all controls.
inline bool validComponentLayout(const ComponentLayoutRequest& request) noexcept {
    if(request.size!=sizeof(request) || request.version!=1 || request.count>maxLayoutGroups || request.gap>96 || request.flags>1 || (request.flags && request.count))return false;
    for(unsigned i=0;i<request.count;++i) {
        const auto& entry=request.entries[i];
        const auto end=std::find(std::begin(entry.id),std::end(entry.id),L'\0');
        if(end==std::end(entry.id) || entry.zone>2 || taskbar::componentFromId({entry.id,static_cast<size_t>(end-entry.id)})==taskbar::Component::Unknown)return false;
        for(unsigned j=0;j<i;++j)if(!wcscmp(entry.id,request.entries[j].id))return false;
    }
    return true;
}
struct ComponentLayoutReport {
    std::uint32_t size=sizeof(ComponentLayoutReport),version=1;
    HRESULT status=E_FAIL;
    std::uint32_t bound{},items{},passes{},failurePhase{},failureItem{};
};
}
