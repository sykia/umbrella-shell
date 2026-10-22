// Owned-process experiment only: never attach this layout to Explorer.
#pragma once
#include "AttachedLayout.h"
#include "ComponentLayoutRequest.h"
#include "core/taskbar/Reorder.h"
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <memory>

namespace umbrella::explorer::component_layout {
using namespace winrt::Windows::UI::Xaml;
using Value=winrt::Windows::Foundation::IInspectable;
class Session {
    friend struct ComponentLayoutFixture;
    struct Binding {
        UIElement element{nullptr};FrameworkElement parent{nullptr};
        taskbar::Component role=taskbar::Component::Unknown;
        winrt::Windows::Foundation::Rect slot{};
        taskbar::Rectangle measured;
    };
    struct Control {
        bool active=true,writing=false;
        FrameworkElement root{nullptr},repeater{nullptr};Controls::StackPanel tray{nullptr};
        ComponentLayoutRequest request;
        HRESULT status=E_PENDING;
        unsigned passes=0,generation=0,appliedGeneration=~0u;
        unsigned failurePhase=0,failureItem=0;
        std::vector<Binding> bindings;
        struct Role {UIElement item{nullptr};taskbar::Component role;};
        std::vector<Role> roles;
        std::vector<taskbar::Move> confirmed;
    };
    std::shared_ptr<Control> control_;
    winrt::com_ptr<layout_contract::Attached> attached_;
    Value layout_{nullptr},original_{nullptr};
    winrt::event_token updated_{};bool subscribed_=false;

    static void pin() noexcept {
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&pin),&module);
    }
    static taskbar::Component role(const UIElement& item) {
        std::vector<DependencyObject> pending{item};taskbar::Component found=taskbar::Component::Unknown;
        unsigned visited=0;
        while(!pending.empty()) {
            if(++visited>96)winrt::throw_hresult(E_BOUNDS);
            auto node=pending.back();pending.pop_back();
            if(auto element=node.try_as<FrameworkElement>()) {
                const auto detected=taskbar::identify(winrt::get_class_name(element).c_str(),element.Name().c_str(),Automation::AutomationProperties::GetAutomationId(element).c_str());
                if(detected!=taskbar::Component::Unknown) {
                    if(found!=taskbar::Component::Unknown && found!=detected)winrt::throw_hresult(E_INVALIDARG);
                    found=detected;
                }
            }
            const int count=Media::VisualTreeHelper::GetChildrenCount(node);
            if(count<0 || pending.size()+count>96)winrt::throw_hresult(E_BOUNDS);
            for(int i=0;i<count;++i)pending.push_back(Media::VisualTreeHelper::GetChild(node,i));
        }
        return found;
    }
    static taskbar::Rectangle bounds(const Control& c,const UIElement& item) {
        const auto element=item.as<FrameworkElement>();
        const auto box=element.TransformToVisual(c.root).TransformBounds({0,0,static_cast<float>(element.ActualWidth()),static_cast<float>(element.ActualHeight())});
        return {box.X,box.Y,box.Width,box.Height};
    }
    static bool hit(const Control& c,const UIElement& item,taskbar::Rectangle box) {
        const auto point=winrt::Windows::Foundation::Point{static_cast<float>(box.x+box.width/2),static_cast<float>(box.y+box.height/2)};
        const auto hostPoint=c.root.TransformToVisual(nullptr).TransformPoint(point);
        for(auto candidate:Media::VisualTreeHelper::FindElementsInHostCoordinates(hostPoint,c.root)) {
            DependencyObject node=candidate;
            for(unsigned depth=0;node && depth<16;++depth) {
                if(node==item)return true;
                node=Media::VisualTreeHelper::GetParent(node);
            }
        }
        return false;
    }
    static void collect(Control& c,const FrameworkElement& parent) {
        const int count=Media::VisualTreeHelper::GetChildrenCount(parent);
        if(count<0 || count>64)winrt::throw_hresult(E_BOUNDS);
        for(int i=0;i<count;++i) {
            auto item=Media::VisualTreeHelper::GetChild(parent,i).try_as<FrameworkElement>();
            if(!item || item.Visibility()!=Visibility::Visible || item.ActualWidth()<=0 || item.ActualHeight()<=0)continue;
            const auto box=bounds(c,item);
            if(!taskbar::valid(box) || !taskbar::intersects(box,{0,0,c.root.ActualWidth(),c.root.ActualHeight()}))continue;
            auto cached=std::find_if(c.roles.begin(),c.roles.end(),[&](const auto& entry){return entry.item==item;});
            if(cached==c.roles.end()) {
                if(c.roles.size()>=128)winrt::throw_hresult(E_BOUNDS);
                // Cache only structural component identity, never app labels.
                c.roles.push_back({item,role(item)});cached=std::prev(c.roles.end());
            }
            if(c.bindings.size()>=taskbar::maxReorderItems)winrt::throw_hresult(E_BOUNDS);
            c.bindings.push_back({item,parent,cached->role,Controls::Primitives::LayoutInformation::GetLayoutSlot(item),box});
        }
    }
    struct Adapter {
        Control& c;
        bool read(std::uint64_t id,taskbar::Rectangle& box) const {
            if(!id || id>c.bindings.size())return false;
            const auto& binding=c.bindings[id-1];
            if(!binding.element.XamlRoot() || binding.element.XamlRoot()!=c.root.XamlRoot() || Media::VisualTreeHelper::GetParent(binding.element)!=binding.parent)return false;
            box=bounds(c,binding.element);return taskbar::valid(box);
        }
        bool write(std::uint64_t id,taskbar::Rectangle target) const {
            if(!id || id>c.bindings.size() || !taskbar::inside(target,c.root.ActualWidth(),c.root.ActualHeight()))return false;
            const auto& binding=c.bindings[id-1];auto slot=binding.slot;
            slot.X+=static_cast<float>(target.x-binding.measured.x);slot.Y+=static_cast<float>(target.y-binding.measured.y);
            binding.element.Arrange(slot);return true;
        }
        bool accept(std::uint64_t id) const {
            taskbar::Rectangle box;
            const bool accepted=read(id,box) && c.bindings[id-1].element.IsHitTestVisible() && hit(c,c.bindings[id-1].element,box);
            if(!accepted){c.failurePhase=4;c.failureItem=static_cast<unsigned>(id);}
            return accepted;
        }
    };
    static void apply(Control& c) noexcept {
        if(!c.active || c.writing || !c.request.count)return;
        try {
            // LayoutUpdated is a notification, never a polling timer. A stable
            // verified pass performs no writes and no tree scan.
            if(c.appliedGeneration==c.generation && !c.confirmed.empty()) {
                bool stable=true;
                for(const auto& move:c.confirmed) {
                    if(move.instance>c.bindings.size() || !taskbar::same(bounds(c,c.bindings[move.instance-1].element),move.after)){stable=false;break;}
                }
                if(stable)return;
            }
            c.writing=true;
            struct Finish {bool& flag;~Finish(){flag=false;}} finish{c.writing};
            c.bindings.clear();collect(c,c.repeater);collect(c,c.tray);
            std::erase_if(c.roles,[&](const auto& cached){return std::none_of(c.bindings.begin(),c.bindings.end(),[&](const auto& binding){return binding.element==cached.item;});});
            std::vector<taskbar::MeasuredItem> items;items.reserve(c.bindings.size());
            for(size_t i=0;i<c.bindings.size();++i) {
                const auto& item=c.bindings[i];items.push_back({i+1,item.role,item.measured,item.role!=taskbar::Component::Unknown});
            }
            std::vector<taskbar::Assignment> assignments;assignments.reserve(c.request.count);
            for(unsigned i=0;i<c.request.count;++i)assignments.push_back({taskbar::componentFromId(c.request.entries[i].id),static_cast<taskbar::Zone>(c.request.entries[i].zone)});
            const auto plan=taskbar::plan(items,assignments,c.root.ActualWidth(),c.root.ActualHeight(),c.request.gap);
            if(plan.status!=taskbar::PlanStatus::Ready){c.failurePhase=1;c.failureItem=static_cast<unsigned>(plan.status);c.status=HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);c.active=false;return;}
            Adapter adapter{c};
            const auto result=taskbar::transact(plan.moves,adapter);
            if(result!=taskbar::Transaction::Applied){if(!c.failurePhase)c.failurePhase=5;c.status=result==taskbar::Transaction::RollbackFailed?E_UNEXPECTED:E_ABORT;c.active=false;return;}
            c.confirmed=plan.moves;c.appliedGeneration=c.generation;c.status=S_OK;++c.passes;
        }catch(...){c.status=winrt::to_hresult();c.active=false;}
    }
    bool install(const std::shared_ptr<Control>& c,const Value& creating) {
        attached_=winrt::make_self<layout_contract::Attached>();attached_->original=original_;
        const std::weak_ptr<Control> weak=c;
        attached_->arrangeCallback=[weak](const auto&,auto size) {if(auto state=weak.lock())++state->generation;return size;};
        layout_=attached_->create(creating);control_=c;
        pin();
        updated_=c->root.LayoutUpdated([weak](auto&&,auto&&){if(auto state=weak.lock())apply(*state);});subscribed_=true;
        layout_contract::set(c->repeater,layout_);
        return true;
    }
public:
    Session()=default;Session(const Session&)=delete;Session& operator=(const Session&)=delete;
    ~Session(){reset();}
    HRESULT configure(HWND,const ComponentLayoutRequest& request) noexcept {
        if(!validComponentLayout(request))return E_INVALIDARG;
        if(!request.count && !request.flags)return reset()?S_OK:E_ABORT;
        try {
            if(!control_)return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED); // Fixture must supply its own objects.
            control_->request=request;control_->status=E_PENDING;control_->appliedGeneration=~0u;
            layout_contract::invalidate(layout_,true);control_->repeater.InvalidateMeasure();control_->root.UpdateLayout();
            apply(*control_);
            const auto status=FAILED(attached_->failure)?attached_->failure:
                request.flags ? (attached_->measures && attached_->arranges ? S_OK:E_ABORT) : control_->status;
            if(FAILED(status)) {
                // Preserve bounded diagnostics without retaining failed objects.
                lastReport_=report(status);reset();
            }
            return status;
        }catch(...){const auto error=winrt::to_hresult();reset();return error;}
    }
    bool reset() noexcept {
        if(!control_){layout_=nullptr;original_=nullptr;attached_=nullptr;return true;}
        bool success=true;auto c=control_;c->active=false;
        try {if(subscribed_){c->root.LayoutUpdated(updated_);subscribed_=false;}}
        catch(...){success=false;}
        try {
            // Restore only our layout, never overwrite a rebuilt owned view.
            Value current{nullptr};winrt::check_hresult(repeater::layout(c->repeater,current));
            if(current!=layout_)success=false;
            else {layout_contract::set(c->repeater,original_);c->tray.InvalidateArrange();c->root.InvalidateMeasure();c->root.UpdateLayout();}
        }catch(...){success=false;}
        control_.reset();layout_=nullptr;original_=nullptr;attached_=nullptr;
        return success;
    }
    ComponentLayoutReport report(HRESULT status) const noexcept {
        ComponentLayoutReport result;result.status=status;
        if(control_){result.bound=1;result.items=static_cast<unsigned>(control_->bindings.size());result.passes=control_->passes;result.failurePhase=control_->failurePhase;result.failureItem=control_->failureItem;}
        else if(FAILED(status)){result=lastReport_;result.status=status;result.bound=0;}
        return result;
    }
private:
    ComponentLayoutReport lastReport_;
};
}
