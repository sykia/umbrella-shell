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
    const auto physical=physicalRect({-1200,100,400,80},{-1707,0,1707,960},{-2560,0,2560,1440},1.5);
    require(physical.x==-1799 && physical.y==150 && physical.width==600 && physical.height==120,"Mixed DPI/negative origin conversion");
    for(const Rect work:{Rect{-2560,-300,2560,1440},Rect{0,0,640,480},Rect{0,0,64,48},Rect{0,0,1,1}})
        for(const auto edge:{Edge::bottom,Edge::top,Edge::left,Edge::right})
            for(const double scale:{1.,1.25,1.5,2.,4.}) for(const int count:{1,3,8,100}) {
                const auto layout=previewLayout(work,{work.x,work.y,48,48},edge,count,scale);
                const auto r=layout.bounds;
                require(r.x>=work.x && r.y>=work.y && r.x+r.width<=work.x+work.width && r.y+r.height<=work.y+work.height,"Preview escaped work area");
                require(layout.count>0 && layout.count<=8,"Preview count bound");
                require(previewHitTest(layout,-1,layout.header)==-1 && previewHitTest(layout,0,r.height)==-1,"Preview hit-test accepted outside input");
                for(int i=0;i<layout.count;++i) {
                    const auto cell=layout.cell(i);
                    require(previewHitTest(layout,cell.x+cell.width/2,cell.y+cell.height/2)==i,"Preview window selection mismatch");
                }
            }
    std::cout << "Dock policy invariants passed\n";
}
