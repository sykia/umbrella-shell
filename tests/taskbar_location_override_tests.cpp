#include "platform/windows/explorer/TaskbarLocationOverride.h"
#include <cstdio>
#include <cstdlib>
using umbrella::explorer::location_override::Session;
using Get=HRESULT(WINAPI*)(void*,unsigned*);
void require(bool value,const char* message){if(!value){std::fprintf(stderr,"%s\n",message);std::exit(1);}}
HRESULT WINAPI original(void*,unsigned* value){if(!value)return E_POINTER;*value=3;return S_OK;}
struct Object {void** table;};
unsigned read(Object& object){unsigned result=99;require(SUCCEEDED(reinterpret_cast<Get>(object.table[10])(&object,&result)),"get failed");return result;}
int main(){
 std::array<void*,41> table{};table.fill(reinterpret_cast<void*>(&original));
 Object first{table.data()},second{table.data()};
 Session a,b;
 require(a.bind(&first,table.data(),0),"first bind failed");
 require(read(first)==0 && read(second)==3,"first override changed peer");
 require(b.bind(&second,table.data(),2),"second bind failed");
 require(read(first)==0 && read(second)==2,"independent edges lost");
 require(first.table[6]==table[6] && first.table[40]==table[40],"unrelated ABI changed");
 require(reinterpret_cast<Get>(first.table[10])(&first,nullptr)==E_POINTER,"null output accepted");
 require(!a.set(4) && read(first)==0,"invalid edge changed state");
 require(a.set(1) && read(first)==1 && read(second)==2,"edge update affected peer");
 Session duplicate;require(!duplicate.bind(&first,table.data(),3),"competing override accepted");
 require(a.reset() && read(first)==3 && read(second)==2,"restore affected peer");
 require(b.reset() && first.table==table.data() && second.table==table.data(),"original identity not restored");
 require(a.reset(),"idempotent reset failed");
 std::puts("Passed: independent location reads, unchanged ABI, validation and exact table restoration.");
}
