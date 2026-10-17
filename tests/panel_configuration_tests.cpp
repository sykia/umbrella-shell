#include "core/PanelConfiguration.h"
#include <iostream>
int main() {
    using namespace umbrella::core;
    if (!validPanelEdge("top") || !validPanelEdge("bottom") || !validPanelEdge("left") || !validPanelEdge("right")) return 1;
    for (auto edge : {"", "TOP", "top/left", "bottom ", "unknown"}) if (validPanelEdge(edge)) return 1;
    for (int zone : {-1, 0, 1, 2}) if (!validZone(zone)) return 1;
    for (int zone : {-2147483647, -2, 3, 2147483647}) if (validZone(zone)) return 1;
    if (!validModule("workspaces") || !validModule("keyboard") || !validModule("volume") || validModule("../startup") || validModule("")) return 1;
    std::cout << "Panel configuration rejects unknown edges/modules and out-of-range zones\n";
}
