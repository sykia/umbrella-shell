#pragma once
#include "TaskbarPlacementProtocol.h"
#include <charconv>
#include <string_view>

namespace umbrella::explorer {
struct PlacementCommand {ComponentLayoutRequest request;char device[64]{};bool statusOnly=false;};
inline bool parsePlacementCommand(std::string_view line,PlacementCommand& result) noexcept {
    result={};
    if(line.empty() || line.size()>1023 || line.back()!='\n' || line.find('\0')!=line.npos)return false;
    auto token=[&]() {
        const auto begin=line.find_first_not_of(" \t\r\n");
        if(begin==line.npos){line={};return std::string_view{};}
        line.remove_prefix(begin);const auto end=line.find_first_of(" \t\r\n");
        const auto word=line.substr(0,end);line.remove_prefix(word.size());return word;
    };
    const auto command=token();
    if(command!="slots" && command!="slots-status")return false;
    const auto device=token();constexpr std::string_view prefix="\\\\.\\DISPLAY";
    if(!device.starts_with(prefix) || device.size()<=prefix.size() || device.size()>32 ||
        device.substr(prefix.size()).find_first_not_of("0123456789")!=device.npos)return false;
    std::copy(device.begin(),device.end(),result.device);
    if(command=="slots-status"){result.statusOnly=true;return token().empty();}
    const auto gap=token();
    if(gap=="reset")return token().empty();
    if(gap.empty())return false;
    const auto parsed=std::from_chars(gap.data(),gap.data()+gap.size(),result.request.gap);
    if(gap.empty() || parsed.ec!=std::errc{} || parsed.ptr!=gap.data()+gap.size() || result.request.gap>96)return false;
    for(auto word=token();!word.empty();word=token()) {
        if(result.request.count>=maxLayoutGroups)return false;
        const auto colon=word.find(':');
        if(colon==word.npos || !colon || colon>=24 || word.size()!=colon+2 || word.back()<'0' || word.back()>'3')return false;
        auto& entry=result.request.entries[result.request.count++];entry.zone=word.back()-'0';
        for(size_t i=0;i<colon;++i){if(word[i]<'a' || word[i]>'z')return false;entry.id[i]=word[i];}
    }
    return result.request.count && validComponentLayout(result.request);
}
}
