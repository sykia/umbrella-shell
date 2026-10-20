#include "core/DockPolicy.h"
#include <iostream>
#include <cstdlib>
using namespace umbrella::dock;
void require(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
int main() {
    for (const Rect screen : {Rect{0,0,2560,1440},Rect{-2560,0,2560,1440},Rect{2560,-300,1707,960}})
        for (const auto edge : {Edge::bottom,Edge::top,Edge::left,Edge::right})
            for (const int length : {1,64,300,40000}) for (const int margin : {-1,0,12,4000}) for (const int align : {0,1,2}) {
                const auto r = geometry(screen,edge,length,80,margin,align);
                require(r.x >= screen.x && r.y >= screen.y && r.width > 0 && r.height > 0 &&
                    r.x+r.width <= screen.x+screen.width && r.y+r.height <= screen.y+screen.height,"Dock escaped a monitor");
            }
    require(!shouldHide(1,true,false,true,true,true),"Pointer must retain the dock");
    require(!shouldHide(1,false,true,true,true,true),"An open menu must retain the dock");
    require(shouldHide(0,false,false,false,true,true),"Fullscreen suppression");
    require(!shouldHide(0,false,false,true,false,true),"Always visible policy");
    require(!shouldHide(2,false,false,false,false,true),"Smart hide without overlap");
    require(shouldHide(2,false,false,true,false,true),"Smart hide overlap");
    require(orderedIdentities({"a","b","a"},{"b","c","","c"}) == std::vector<std::string>({"a","b","c"}),"Pinned order/deduplication");
    std::cout << "Dock policy invariants passed\n";
}
