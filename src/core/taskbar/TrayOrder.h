#pragma once
#include <algorithm>
#include <array>
#include <optional>

namespace umbrella::taskbar {
// Wire order is stable: overflow/icons, input indicators, Quick Settings, clock.
// Lexicographic ranks preserve the original 0=reset, 1=clock/Quick Settings swap.
using TrayOrder=std::array<unsigned,4>;
inline std::optional<TrayOrder> trayOrder(unsigned code) noexcept {
    if(code>=24)return {};
    TrayOrder order{0,1,2,3};
    for(unsigned i=0;i<code;++i)std::next_permutation(order.begin(),order.end());
    return order;
}
inline std::optional<unsigned> trayOrderCode(const TrayOrder& order) noexcept {
    TrayOrder next{0,1,2,3};
    for(unsigned code=0;code<24;++code) {
        if(next==order)return code;
        std::next_permutation(next.begin(),next.end());
    }
    return {};
}
}
