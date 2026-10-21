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
    packet.version=2;
    require(!validComponentSnapshot(packet),"v2 snapshot accepted as v3");
    packet.version=3; packet.size=sizeof(packet)-sizeof(ComponentSnapshotEntry);
    require(!validComponentSnapshot(packet),"Truncated module snapshot accepted");
    packet.size=sizeof(packet); packet.count=maxComponentSnapshot+1;
    require(!validComponentSnapshot(packet),"Packet could index beyond fixed entries");
    packet.count=maxComponentSnapshot; packet.visited=512; packet.status=S_OK;
    require(validComponentSnapshot(packet),"Maximum bounded snapshot rejected");
    packet.visited=513;
    require(!validComponentSnapshot(packet),"Unbounded native traversal reported as valid");
    packet={}; packet.count=1;
    auto& owner=packet.entries[0]; owner.ownerStatus=S_OK;
    require(!validComponentSnapshot(packet),"Successful owner without identities/index accepted");
    owner.ownerId=owner.containerId=1; owner.modelIndex=0; owner.ownerDepth=7;
    require(validComponentSnapshot(packet),"Bounded wrapped owner rejected");
    owner.ownerDepth=8;
    require(!validComponentSnapshot(packet),"Unbounded owner ancestry accepted");
    owner.ownerDepth=0; owner.containerId=2;
    require(!validComponentSnapshot(packet),"Out-of-snapshot container identity accepted");
    owner.containerId=1; owner.ownerStatus=E_ABORT;
    require(!validComponentSnapshot(packet),"Failed owner retained apparently usable model identity");
    std::cout<<"Passed: v1/v2/v3 mismatch, truncated snapshot, array/traversal bounds and explicit failure framing. No Explorer.\n";
}
