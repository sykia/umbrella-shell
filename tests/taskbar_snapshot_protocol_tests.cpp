#include "platform/windows/explorer/TaskbarModuleProtocol.h"
#include "platform/windows/explorer/TaskbarPlacementCommand.h"
#include <cstdlib>
#include <iostream>
using namespace umbrella::explorer;
void require(bool value,const char* message){ if(!value){std::cerr<<message<<'\n';std::exit(1);} }
int main(){
    PlacementCommand command;
    require(parsePlacementCommand("slots \\\\.\\DISPLAY1 8 start:0 clock:2\n",command) && command.request.count==2,"Bounded placement command rejected");
    require(parsePlacementCommand("slots \\\\.\\DISPLAY2 reset\n",command) && !command.request.count,"Explicit placement reset rejected");
    require(parsePlacementCommand("slots-status \\\\.\\DISPLAY1\n",command) && command.statusOnly,"Read-only placement status rejected");
    for(const auto bad:{"layout \\\\.\\DISPLAY1 probe\n","slots \\\\.\\DISPLAY1 8\n","slots \\\\.\\DISPLAY1 8 start:0 start:1\n",
        "slots \\\\.\\DISPLAY1 8 start:3\n","slots \\\\.\\DISPLAY1 97 start:0\n","slots \\\\.\\DISPLAY1 8 invalid:0\n",
        "slots \\\\.\\DISPLAY1 8 desktop:0\n","slots \\\\.\\DISPLAY1 8 desktop:2 clock:2\n",
        "slots \\\\.\\DISPLAYX 8 start:0\n","slots \\\\.\\DISPLAY1 reset clock:0\n","slots-status \\\\.\\DISPLAY1 extra\n",
        "slots \\\\.\\DISPLAY1 8 clock:0"})require(!parsePlacementCommand(bad,command),"Malformed/legacy placement command accepted");
    ComponentLayoutRequest request;
    request.flags=1;require(!validComponentLayout(request),"Retired layout replacement probe reached native placement");
    request={};request.version=2;require(!validComponentLayout(request),"Unknown native placement version accepted");
    ComponentSnapshot packet;
    require(validComponentSnapshot(packet),"Failure report framing rejected");
    packet.version=1;
    require(!validComponentSnapshot(packet),"Old module snapshot ABI accepted by new host");
    packet.version=2;
    require(!validComponentSnapshot(packet),"v2 snapshot accepted as v5");
    packet.version=3;
    require(!validComponentSnapshot(packet),"v3 snapshot accepted with a different entry size");
    packet.version=4;
    require(!validComponentSnapshot(packet),"v4 snapshot accepted without source contracts");
    packet.version=5; packet.size=sizeof(packet)-sizeof(ComponentSnapshotEntry);
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
    owner.sourceStatus=S_OK;owner.sourceContracts=SourceVectorView|SourceNotifyCollection;
    require(validComponentSnapshot(packet),"Read-only indexed source metadata rejected");
    owner.sourceContracts|=0x80000000u;
    require(!validComponentSnapshot(packet),"Unknown source interface flags accepted");
    owner.sourceContracts=SourceVectorView;owner.sourceStatus=E_ABORT;
    require(!validComponentSnapshot(packet),"Failed source query retained apparent capabilities");
    owner.sourceContracts=0;owner.sourceStatus=E_NOINTERFACE;
    owner.ownerDepth=8;
    require(!validComponentSnapshot(packet),"Unbounded owner ancestry accepted");
    owner.ownerDepth=0; owner.containerId=2;
    require(!validComponentSnapshot(packet),"Out-of-snapshot container identity accepted");
    owner.containerId=1; owner.ownerStatus=E_ABORT;
    require(!validComponentSnapshot(packet),"Failed owner retained apparently usable model identity");
    owner.ownerId=owner.containerId=0;owner.modelIndex=-1;owner.sourceStatus=S_OK;
    require(!validComponentSnapshot(packet),"Source metadata without a valid owner accepted");
    std::cout<<"Passed: v1-v5 mismatch, snapshot bounds, source contracts and explicit failure framing. No Explorer.\n";
}
