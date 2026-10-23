#include "core/taskbar/Reorder.h"
#include "core/taskbar/ModelOrder.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <chrono>
#include <stdexcept>
using namespace umbrella::taskbar;
void require(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
struct Adapter {
    std::map<std::uint64_t, Rectangle> elements;
    unsigned writes = 0, failAt = 0;
    bool throwing = false, ignored = false, rollbackBroken = false, acceptAll = true;
    bool read(std::uint64_t id, Rectangle& result) { if (!elements.contains(id)) return false; result = elements[id]; return true; }
    bool write(std::uint64_t id, Rectangle value) {
        ++writes;
        if (rollbackBroken && writes > failAt) return false;
        if (!ignored) elements[id] = value;
        if (writes == failAt) { if (throwing) throw std::runtime_error("partially completed setter"); return false; }
        return true;
    }
    bool accept(std::uint64_t) { return acceptAll; }
};
int main() {
    const std::vector<MeasuredItem> conditionalTray{
        {1,Component::TrayOverflow,{2308,0,32,48},true},
        {2,Component::SystemIndicators,{2340,0,32,48},false},
        {3,Component::InputIndicators,{2372,0,44,48},true},
        {4,Component::QuickSettings,{2416,0,60,48},true},
        {5,Component::Clock,{2476,0,72,48},true},
        {6,Component::Desktop,{2548,0,12,48},true},
        {7,Component::Start,{970,0,45,48},true}};
    const std::vector<Assignment> trayRequest{{Component::TrayOverflow,Zone::Right},{Component::InputIndicators,Zone::Right},
        {Component::QuickSettings,Zone::Right},{Component::Clock,Zone::Right},{Component::Desktop,Zone::Right},{Component::Start,Zone::Left}};
    const auto conditional=plan(conditionalTray,trayRequest,2560,48,8);
    require(conditional.status==PlanStatus::Ready,"Visible stationary system group blocked an unrelated Start move");
    for(const auto& move:conditional.moves)require(move.instance!=2 && !intersects(move.after,conditionalTray[1].bounds),"Conditional system group was moved or covered");
    struct Model {
        std::vector<int> values{1,2,3,4};
        unsigned writes=0, failAt=0;
        bool partial=false, foreign=false, reject=false;
        bool snapshot(std::vector<int>& out) { out=values; return true; }
        bool remove(unsigned index) {
            ++writes;
            if (writes==failAt && !partial) return false;
            values.erase(values.begin()+index);
            if (foreign) { values.push_back(99); return false; }
            if (writes==failAt) throw std::runtime_error("notification failed after removal");
            return true;
        }
        bool insert(unsigned index,int value) {
            ++writes;
            if (writes==failAt && !partial) return false;
            values.insert(values.begin()+index,value);
            if (writes==failAt) throw std::runtime_error("notification failed after insertion");
            return true;
        }
        bool verify(const std::vector<int>& order) { return !reject || order==std::vector<int>{1,2,3,4}; }
    };
    const std::vector<int> originalOrder{1,2,3,4}, requestedOrder{4,3,2,1};
    Model model;
    require(reorderModel(originalOrder,requestedOrder,model)==Transaction::Applied && model.values==requestedOrder,"model order did not apply");
    model={};
    require(reorderModel(originalOrder,originalOrder,model)==Transaction::Applied && !model.writes,"model no-op sent notifications");
    for(unsigned failure=1;failure<=6;++failure) for(bool partial : {false,true}) {
        model={}; model.failAt=failure; model.partial=partial;
        require(reorderModel(originalOrder,requestedOrder,model)==Transaction::RolledBack && model.values==originalOrder,"failed model write lost original order/removed object");
    }
    model={}; model.reject=true;
    require(reorderModel(originalOrder,requestedOrder,model)==Transaction::RolledBack && model.values==originalOrder,"native model verification failure committed");
    model={}; model.values.push_back(5);
    require(reorderModel(originalOrder,requestedOrder,model)==Transaction::Stale && !model.writes,"dynamic model changed after snapshot was overwritten");
    model={}; model.foreign=true;
    require(reorderModel(originalOrder,requestedOrder,model)==Transaction::RollbackFailed && model.writes==1 && model.values.back()==99,"reentrant foreign update overwritten by rollback");
    model={};
    require(reorderModel(originalOrder,std::vector<int>{1,2,2,4},model)==Transaction::Invalid && !model.writes,"duplicate model identity accepted");
    std::cout << "Passed: logical model permutation, no-op, failures before/after all six mutations, rollback of removed objects, stale dynamic models and reentrant update preservation.\n";
    const std::array items{
        MeasuredItem{1, Component::Widgets, {0,0,100,48}, true},
        MeasuredItem{2, Component::Start, {300,0,44,48}, true},
        MeasuredItem{3, Component::Search, {344,0,120,48}, true},
        MeasuredItem{4, Component::TaskView, {464,0,44,48}, true},
        MeasuredItem{5, Component::Applications, {508,0,44,48}, true},
        MeasuredItem{6, Component::Applications, {556,0,44,48}, true}};
    const std::array request{
        Assignment{Component::Search, Zone::Left}, Assignment{Component::Widgets, Zone::Left},
        Assignment{Component::Applications, Zone::Center}, Assignment{Component::TaskView, Zone::Right},
        Assignment{Component::Start, Zone::Right}};
    const auto layout = plan(items, request, 1000,48,8);
    require(layout.status == PlanStatus::Ready && layout.moves.size() == 6, "multi-zone reorder refused");
    require(layout.moves[0].instance == 3 && layout.moves[0].after.x == 0, "same-zone order lost");
    require(layout.moves[1].instance == 1 && layout.moves[1].after.x == 128, "group gap lost");
    require(layout.moves[3].after.x-layout.moves[2].after.x == 48, "native task-list internal offsets changed");
    require(layout.moves[5].after.x == 956, "right boundary not anchored");
    Adapter original; for (const auto item : items) original.elements[item.instance] = item.bounds;
    auto native = original;
    require(transact(layout.moves,native) == Transaction::Applied, "batch apply failed");
    for (const auto move : layout.moves) require(same(native.elements[move.instance],move.after), "wrong committed native position");
    native = original; native.failAt = 3;
    require(transact(layout.moves,native) == Transaction::RolledBack && native.writes == 6, "partial setter failure did not reverse all touched elements");
    for (const auto item : items) require(same(native.elements[item.instance],item.bounds), "rollback lost original geometry");
    native = original; native.failAt = 3; native.throwing = true;
    require(transact(layout.moves,native) == Transaction::RolledBack, "throwing native setter escaped rollback");
    native = original; native.ignored = true;
    require(transact(layout.moves,native) == Transaction::RolledBack, "ignored write committed");
    native = original; native.acceptAll = false;
    require(transact(layout.moves,native) == Transaction::RolledBack, "native verification refusal committed");
    native = original; native.failAt = 3; native.rollbackBroken = true;
    require(transact(layout.moves,native) == Transaction::RollbackFailed && native.writes == 6, "rollback failure concealed or stopped later restores");
    native = original; native.elements.erase(2);
    require(transact(layout.moves,native) == Transaction::Stale && native.writes == 0, "unloaded object mutated peers");
    native = original; native.elements[2].x += 10;
    require(transact(layout.moves,native) == Transaction::Stale && native.writes == 0, "stale snapshot mutated peers");
    auto blocked = items; blocked[2].movable = false;
    require(plan(blocked,request,1000,48,8).status == PlanStatus::Unsupported, "discovery granted native capability");
    require(plan(items,request,300,48,8).status != PlanStatus::Ready, "overflow accepted");
    const std::array tray{Rectangle{940,0,60,48}};
    const auto collision = plan(items,request,1000,48,8,tray);
    require(collision.status == PlanStatus::Ready,"Right-edge obstacle was not reserved during packing");
    for(const auto& move:collision.moves)require(!intersects(move.after,tray[0]),"Stationary tray obstacle was covered");
    const std::array fullObstacle{Rectangle{0,0,1000,48}};
    const auto noRoom=plan(items,request,1000,48,8,fullObstacle);
    require(noRoom.status==PlanStatus::Collision && noRoom.moves.empty(),"Rejected plan exposed writable moves");
    const std::array absent{Assignment{Component::Clock,Zone::Right}};
    require(plan(items,absent,1000,48,8).status == PlanStatus::Missing, "absent conditional component invented");
    const std::array duplicate{request[0],request[0]};
    require(plan(items,duplicate,1000,48,8).status == PlanStatus::Invalid, "duplicate role accepted");
    auto changed = std::vector<MeasuredItem>(items.begin(),items.end());
    changed.push_back({7,Component::Applications,{604,0,44,48},true});
    const auto expanded = plan(changed,request,1000,48,8);
    require(expanded.status == PlanStatus::Ready && expanded.moves.size() == 7, "new task-list instance not included");
    changed.erase(changed.begin()+4);
    require(plan(changed,request,1000,48,8).status == PlanStatus::Ready, "closed task-list instance broke group");
    const auto secondMonitor = plan(items,request,1600,48,8);
    require(secondMonitor.status == PlanStatus::Ready && secondMonitor.moves[5].after.x == 1556 &&
        layout.moves[5].after.x == 956, "monitor geometry was shared or cached across panels");
    native = original;
    const std::array unchanged{Move{1,items[0].bounds,items[0].bounds}};
    require(transact(unchanged,native) == Transaction::Applied && native.writes == 0, "no-op wrote native properties");
    auto invalid = layout.moves; invalid[1].after = invalid[0].after;
    require(transact(invalid,native) == Transaction::Invalid && native.writes == 0, "overlapping transaction mutated native state");
    auto ambiguous = items; ambiguous[2].role = Component::Start;
    const std::array startOnly{Assignment{Component::Start,Zone::Left}};
    require(plan(ambiguous,startOnly,1000,48,8).status == PlanStatus::Ambiguous, "duplicate singleton granted capability");
    std::vector<MeasuredItem> maximum;
    for (unsigned i=0; i<maxReorderItems; ++i) maximum.push_back({i+1,Component::Applications,{44.0*i,0,44,48},true});
    const std::array appsOnly{Assignment{Component::Applications,Zone::Center}};
    const auto started = std::chrono::steady_clock::now();
    for (unsigned i=0; i<1000; ++i) {
        const auto measured = plan(maximum,appsOnly,4096,48,8);
        require(measured.status == PlanStatus::Ready && measured.moves.size() == maxReorderItems, "bounded maximum fixture failed");
    }
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    maximum.push_back({65,Component::Applications,{2816,0,44,48},true});
    require(plan(maximum,appsOnly,4096,48,8).status == PlanStatus::Invalid, "unbounded item count accepted");
    std::cout << "Passed: batch order/zones, dynamic app groups, capability/bounds, stale snapshots, partial/throwing/ignored setters and reverse rollback.\n";
    std::cout << "Planner fixture only: 1000 layouts x 64 items in " << duration << " ms; no native/drag latency measured.\n";
}
