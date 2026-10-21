#include "platform/windows/explorer/TaskbarModuleProtocol.h"
#include <cstdlib>
#include <iostream>
using namespace umbrella::explorer;
void require(bool value,const char* message){ if(!value){std::cerr<<message<<'\n';std::exit(1);} }
int main(){
    ComponentSnapshot packet;
    require(validComponentSnapshot(packet),"Failure report framing rejected");
    packet.version=1;
    require(!validComponentSnapshot(packet),"Old module snapshot ABI accepted by new host");
    packet.version=2; packet.size=sizeof(packet)-sizeof(ComponentSnapshotEntry);
    require(!validComponentSnapshot(packet),"Truncated module snapshot accepted");
    packet.size=sizeof(packet); packet.count=maxComponentSnapshot+1;
    require(!validComponentSnapshot(packet),"Packet could index beyond fixed entries");
    packet.count=maxComponentSnapshot; packet.visited=512; packet.status=S_OK;
    require(validComponentSnapshot(packet),"Maximum bounded snapshot rejected");
    packet.visited=513;
    require(!validComponentSnapshot(packet),"Unbounded native traversal reported as valid");
    std::cout<<"Passed: v1/v2 mismatch, truncated snapshot, array/traversal bounds and explicit failure framing. No Explorer.\n";
}
