// Places original layout slots after the unchanged native owner. No Layout replacement, source mutation or reparenting.
#pragma once
#include "TaskbarRepeaterAccess.h"
#include "TaskbarContextMenu.h"
#include "TaskbarPlacementProtocol.h"
#include "core/taskbar/Reorder.h"
#include "core/taskbar/SurfaceGeometry.h"
#include "TaskbarSourceCapabilities.h"
#include <shellapi.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <memory>
#include <functional>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace umbrella::explorer::component_layout {
using namespace winrt::Windows::UI::Xaml;
using Value=winrt::Windows::Foundation::IInspectable;
// Prepared by the existing appearance owner; prepare must not alter Windows.
// Slots and the surface mask are verified before either may be acknowledged.
struct ClipUpdate {
    virtual ~ClipUpdate()=default;
    virtual bool apply() noexcept=0;
    virtual bool rollback() noexcept=0;
};
using ClipFactory=std::function<std::unique_ptr<ClipUpdate>(std::span<const taskbar::Rectangle>,double,double)>;
class Session {
    friend struct ComponentLayoutFixture;
    struct Binding {
        UIElement element{nullptr};FrameworkElement parent{nullptr};
        taskbar::Component role=taskbar::Component::Unknown;
        winrt::Windows::Foundation::Rect slot{};
        taskbar::Rectangle measured;
        winrt::Windows::Foundation::Rect committedSlot{};
    };
    struct Control {
        bool active=true,writing=false;
        HWND notificationOwner{},notificationTarget{};
        unsigned notificationSerial{};
        HWND panel{};
        FrameworkElement root{nullptr},repeater{nullptr};Controls::StackPanel tray{nullptr};
        Value layout{nullptr},source{nullptr};
        int repeaterChildren=-1,trayChildren=-1;
        ComponentLayoutRequest request;
        taskbar::Rectangle viewport; // Root-relative DIP, supplied by the owning geometry transaction.
        ClipFactory clipFactory;
        taskbar::SurfaceSpec surface;
        UINT appliedDpi=0,epochDpi=0;
        bool geometryQueued=false;
        double viewportRootWidth=0,viewportRootHeight=0;
        HRESULT status=E_PENDING;
        unsigned passes=0,generation=0,appliedGeneration=~0u;
        unsigned attempts=0;
        unsigned epochAttempts=0,epochGeneration=~0u,windowAttempts=0;
        double epochWidth=-1,epochHeight=-1;
        ULONGLONG windowStart=0;
        bool budgetResetQueued=false;
        std::weak_ptr<Control> self;
        unsigned failurePhase=0,failureItem=0;
        std::vector<Binding> bindings;
        struct Role {UIElement item{nullptr};taskbar::Component role;};
        std::vector<Role> roles;
        std::vector<taskbar::Move> confirmed;
    };
    std::shared_ptr<Control> control_;
    Interop::INotifyCollectionChanged notifications_{nullptr};
    winrt::Windows::Foundation::Collections::IObservableVector<Value> vector_{nullptr};
    winrt::event_token changed_{};
    bool modelSubscribed_=false;
    winrt::event_token updated_{};bool subscribed_=false;
    winrt::event_token traySize_{};bool traySizeSubscribed_=false;

    static void pin() noexcept {
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&pin),&module);
    }
    static taskbar::Component role(const UIElement& item) {
        if(auto element=item.try_as<FrameworkElement>()) {
            const auto outer=taskbar::identify(winrt::get_class_name(element).c_str(),element.Name().c_str(),Automation::AutomationProperties::GetAutomationId(element).c_str());
            if(outer!=taskbar::Component::Unknown)return outer;
        }
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
        double x=0,y=0;DependencyObject node=item;
        for(unsigned depth=0;node && node!=c.root && depth<64;++depth) {
            const auto current=node.as<UIElement>();const auto offset=current.ActualOffset();
            x+=offset.x;y+=offset.y;node=Media::VisualTreeHelper::GetParent(node);
        }
        if(node!=c.root)winrt::throw_hresult(E_ABORT);
        // ActualOffset is the arrange result. TransformToVisual can contain a
        // transient compositor/theme translation from an earlier placement.
        return {x,y,element.ActualWidth(),element.ActualHeight()};
    }
    static taskbar::Rectangle presentedBounds(const Control& c,const UIElement& item) {
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
        uint32_t modelSize=0;
        if(parent==c.repeater) {
            using namespace winrt::Windows::Foundation::Collections;
            if(auto view=c.source.try_as<IVectorView<Value>>())modelSize=view.Size();
            else if(auto vector=c.source.try_as<IVector<Value>>())modelSize=vector.Size();
            else if(auto bindable=c.source.try_as<Interop::IBindableVector>())modelSize=bindable.Size();
            else winrt::throw_hresult(E_NOINTERFACE);
        }
        for(int i=0;i<count;++i) {
            auto item=Media::VisualTreeHelper::GetChild(parent,i).try_as<FrameworkElement>();
            if(!item || item.Visibility()!=Visibility::Visible || item.ActualWidth()<=0 || item.ActualHeight()<=0)continue;
            if(parent==c.repeater) {
                int index=-1;winrt::check_hresult(repeater::elementIndex(parent,item,index));
                // Explorer keeps retiring/recycled visuals during app-close
                // animations. They are not members of the current model, even
                // if their old rectangle and Visibility still look live.
                if(index<0 || static_cast<uint32_t>(index)>=modelSize)continue;
            }
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
    static void fail(Control& c,HRESULT status) noexcept {
        c.status=status;c.active=false;c.confirmed.clear();
        if(c.notificationOwner) {
            PostMessageW(c.notificationOwner,placementLostMessage,c.notificationSerial,reinterpret_cast<LPARAM>(c.notificationTarget));
            c.notificationOwner=nullptr;
        }
        try {
            Value layout{nullptr},source{nullptr};
            winrt::check_hresult(repeater::layout(c.repeater,layout));
            winrt::check_hresult(repeater::itemsSource(c.repeater,source));
            if(layout!=c.layout || source!=c.source)return;
            // Request one ordinary owner pass; never recurse into UpdateLayout
            // from LayoutUpdated, or retry a rejected layout on every frame.
            c.tray.InvalidateArrange();c.repeater.InvalidateArrange();c.root.InvalidateMeasure();
        }catch(...){} // Preserve the original failure; reset still reports its own result.
    }
    struct Adapter {
        Control& c;
        ClipUpdate* clip=nullptr;
        mutable bool clipAttempted=false;
        unsigned generation=c.generation;
        bool read(std::uint64_t id,taskbar::Rectangle& box) const {
            if(!id || id>c.bindings.size() || generation!=c.generation)return false;
            const auto& binding=c.bindings[id-1];
            if(!binding.element.XamlRoot() || binding.element.XamlRoot()!=c.root.XamlRoot() || Media::VisualTreeHelper::GetParent(binding.element)!=binding.parent)return false;
            box=bounds(c,binding.element);return taskbar::valid(box);
        }
        bool write(std::uint64_t id,taskbar::Rectangle target) const {
            if(!id || id>c.bindings.size() || generation!=c.generation || !taskbar::inside(target,c.root.ActualWidth(),c.root.ActualHeight()))return false;
            const auto& binding=c.bindings[id-1];auto slot=binding.slot;
            slot.X+=static_cast<float>(target.x-binding.measured.x);slot.Y+=static_cast<float>(target.y-binding.measured.y);
            binding.element.Arrange(slot);return true;
        }
        bool accept(std::uint64_t id) const {
            if(clip && !clipAttempted) {
                clipAttempted=true;
                if(!clip->apply()){c.failurePhase=7;return false;}
            }
            taskbar::Rectangle box;
            const bool accepted=read(id,box) && c.bindings[id-1].element.IsHitTestVisible() && hit(c,c.bindings[id-1].element,presentedBounds(c,c.bindings[id-1].element));
            if(!accepted){c.failurePhase=4;c.failureItem=static_cast<unsigned>(id);}
            return accepted;
        }
    };
    static void apply(Control& c) noexcept {
        if(!c.active || c.writing || !c.request.count)return;
        try {
            Value layout{nullptr},source{nullptr};
            winrt::check_hresult(repeater::layout(c.repeater,layout));
            winrt::check_hresult(repeater::itemsSource(c.repeater,source));
            if(layout!=c.layout || source!=c.source || !c.root.Dispatcher().HasThreadAccess() ||
                !c.root.XamlRoot() || c.root.XamlRoot()!=c.repeater.XamlRoot() || c.root.XamlRoot()!=c.tray.XamlRoot())
                winrt::throw_hresult(E_ABORT);
            taskbar::Rectangle measuredViewport;
            const auto dpi=c.panel?GetDpiForWindow(c.panel):0;
            if(c.panel && (c.surface.enabled?!measureSurface(c,measuredViewport):!standardPanel(c.panel))) {
                c.failurePhase=8;fail(c,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));return;
            }
            // LayoutUpdated is a notification, never a polling timer. A stable
            // verified pass performs no writes and no tree scan.
            if(c.appliedGeneration==c.generation && !c.confirmed.empty() &&
                c.appliedDpi==dpi &&
                c.epochWidth==c.root.ActualWidth() && c.epochHeight==c.root.ActualHeight() &&
                c.repeaterChildren==Media::VisualTreeHelper::GetChildrenCount(c.repeater) &&
                c.trayChildren==Media::VisualTreeHelper::GetChildrenCount(c.tray)) {
                bool stable=true;
                for(const auto& move:c.confirmed) {
                    if(move.instance>c.bindings.size()){stable=false;break;}
                    const auto& binding=c.bindings[move.instance-1];
                    const auto slot=Controls::Primitives::LayoutInformation::GetLayoutSlot(binding.element.as<FrameworkElement>());
                    const auto expected=binding.committedSlot;
                    const auto element=binding.element.as<FrameworkElement>();
                    // Native compositor animations may still be travelling to
                    // the committed slot. Re-arranging from animated bounds
                    // fights Explorer and can invalidate an otherwise valid move.
                    if(binding.element.Visibility()!=Visibility::Visible || Media::VisualTreeHelper::GetParent(binding.element)!=binding.parent ||
                        std::abs(element.ActualWidth()-move.after.width)>.5 || std::abs(element.ActualHeight()-move.after.height)>.5 ||
                        !taskbar::same({slot.X,slot.Y,slot.Width,slot.Height},{expected.X,expected.Y,expected.Width,expected.Height})) {stable=false;break;}
                }
                if(stable)return;
            }
            c.writing=true;
            struct Finish {bool& flag;~Finish(){flag=false;}} finish{c.writing};
            c.bindings.clear();collect(c,c.repeater);collect(c,c.tray);
            c.repeaterChildren=Media::VisualTreeHelper::GetChildrenCount(c.repeater);
            c.trayChildren=Media::VisualTreeHelper::GetChildrenCount(c.tray);
            std::erase_if(c.roles,[&](const auto& cached){return std::none_of(c.bindings.begin(),c.bindings.end(),[&](const auto& binding){return binding.element==cached.item;});});
            std::vector<taskbar::MeasuredItem> items;items.reserve(c.bindings.size());
            for(size_t i=0;i<c.bindings.size();++i) {
                const auto& item=c.bindings[i];items.push_back({i+1,item.role,item.measured,
                    item.role!=taskbar::Component::Unknown && item.role!=taskbar::Component::Desktop});
            }
            std::vector<taskbar::Assignment> assignments;assignments.reserve(c.request.count);
            for(unsigned i=0;i<c.request.count;++i) {
                const auto requested=taskbar::componentFromId(c.request.entries[i].id);
                if(requested==taskbar::Component::Desktop)continue; // Preserve the physical edge slot.
                // Missing conditional groups retain placement intent without
                // acquiring a fake slot or object. An empty plan still fails.
                if(std::any_of(c.bindings.begin(),c.bindings.end(),[&](const auto& binding){return binding.role==requested;}))
                    assignments.push_back({requested,static_cast<taskbar::Zone>(c.request.entries[i].zone)});
            }
            auto viewport=c.viewport;
            // Preserve measured edge insets during an owner resize. Reusing the
            // old absolute bottom/right edge rejects valid newly-centred slots.
            if(viewport.width!=0 || viewport.height!=0 || viewport.x!=0 || viewport.y!=0) {
                viewport.width+=c.root.ActualWidth()-c.viewportRootWidth;
                viewport.height+=c.root.ActualHeight()-c.viewportRootHeight;
            }
            if(c.panel && c.surface.enabled)viewport=measuredViewport;
            const auto plan=taskbar::plan(items,assignments,c.root.ActualWidth(),c.root.ActualHeight(),c.request.gap,{},viewport);
            if(plan.status!=taskbar::PlanStatus::Ready){c.failurePhase=1;c.failureItem=static_cast<unsigned>(plan.status);fail(c,HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED));return;}
            // If an owner continually reinstates its positions, give up well
            // before XAML reports a layout cycle. Source reset/resize events must
            // not replenish the budget inside the same dispatcher turn.
            const auto width=c.root.ActualWidth(),height=c.root.ActualHeight();
            if(c.epochGeneration!=c.generation || c.epochWidth!=width || c.epochHeight!=height || c.epochDpi!=dpi) {
                c.epochGeneration=c.generation;c.epochWidth=width;c.epochHeight=height;c.epochAttempts=0;
                c.epochDpi=dpi;
            }
            const auto now=GetTickCount64();
            if(now-c.windowStart>=1000){c.windowStart=now;c.windowAttempts=0;}
            if(c.attempts>=3 || c.epochAttempts>=3 || c.windowAttempts>=8){c.failurePhase=6;fail(c,E_ABORT);return;}
            if(!c.budgetResetQueued) {
                c.budgetResetQueued=true;
                c.root.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low,[weak=c.self] {
                    if(auto state=weak.lock()){state->attempts=0;state->budgetResetQueued=false;}
                });
            }
            ++c.attempts;++c.epochAttempts;++c.windowAttempts;
            std::unique_ptr<ClipUpdate> clip;
            if(c.clipFactory) {
                std::vector<taskbar::Rectangle> destination;destination.reserve(c.bindings.size());
                for(const auto& binding:c.bindings)destination.push_back(binding.measured);
                for(const auto& move:plan.moves)destination[move.instance-1]=move.after;
                clip=c.clipFactory(destination,c.root.ActualWidth(),c.root.ActualHeight());
                if(!clip){c.failurePhase=7;fail(c,E_ABORT);return;}
            }
            Adapter adapter{c,clip.get()};
            auto result=taskbar::transact(plan.moves,adapter);
            if(result!=taskbar::Transaction::Applied && adapter.clipAttempted && !clip->rollback())
                result=taskbar::Transaction::RollbackFailed;
            if(result!=taskbar::Transaction::Applied){if(!c.failurePhase)c.failurePhase=5;fail(c,result==taskbar::Transaction::RollbackFailed?E_UNEXPECTED:E_ABORT);return;}
            for(auto& binding:c.bindings)binding.committedSlot=Controls::Primitives::LayoutInformation::GetLayoutSlot(binding.element.as<FrameworkElement>());
            c.confirmed=plan.moves;c.appliedGeneration=c.generation;c.appliedDpi=dpi;c.status=S_OK;++c.passes;
        }catch(...){fail(c,winrt::to_hresult());}
    }
    static bool standardPanel(HWND hwnd) noexcept {
        RECT rect{};MONITORINFO info{sizeof(info)};APPBARDATA bar{};bar.cbSize=sizeof(bar);bar.hWnd=hwnd;
        return GetDpiForWindow(hwnd)==96 && GetWindowRect(hwnd,&rect) &&
            GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONULL),&info) &&
            rect.left==info.rcMonitor.left && rect.right==info.rcMonitor.right && rect.bottom==info.rcMonitor.bottom && rect.bottom-rect.top==48 &&
            !(SHAppBarMessage(ABM_GETSTATE,&bar)&ABS_AUTOHIDE);
    }
    static bool measureSurface(Control& c,taskbar::Rectangle& viewport) {
        c.failureItem=5;
        RECT rect{};MONITORINFO monitor{sizeof(monitor)};APPBARDATA bar{};bar.cbSize=sizeof(bar);bar.hWnd=c.panel;
        if(!GetWindowRect(c.panel,&rect) || !GetMonitorInfoW(MonitorFromWindow(c.panel,MONITOR_DEFAULTTONULL),&monitor))return false;
        const auto transform=c.root.TransformToVisual(nullptr);
        const auto origin=transform.TransformPoint({0,0}),x=transform.TransformPoint({1,0}),y=transform.TransformPoint({0,1});
        if(std::abs(x.X-origin.X-1)>.001 || std::abs(x.Y-origin.Y)>.001 ||
            std::abs(y.X-origin.X)>.001 || std::abs(y.Y-origin.Y-1)>.001){c.failureItem=8;return false;}
        const auto box=[](const RECT& r){return taskbar::Rectangle{static_cast<double>(r.left),static_cast<double>(r.top),
            static_cast<double>(r.right)-r.left,static_cast<double>(r.bottom)-r.top};};
        const taskbar::SurfaceMeasurement measured{box(rect),box(monitor.rcMonitor),static_cast<double>(GetDpiForWindow(c.panel)),
            c.root.ActualWidth(),c.root.ActualHeight(),c.root.XamlRoot().RasterizationScale(),origin.X,origin.Y,
            (SHAppBarMessage(ABM_GETSTATE,&bar)&ABS_AUTOHIDE)!=0};
        if(taskbar::surfaceViewport(c.surface,measured,viewport))return true;
        const auto scale=measured.dpi/96;
        if(std::abs(measured.rootWidth*scale-measured.panel.width)>1)c.failureItem=1;
        else if(std::abs(measured.rootHeight*scale-measured.panel.height)>1)c.failureItem=2;
        else if(std::abs(measured.rootX)>.01 || std::abs(measured.rootY)>.01)c.failureItem=3;
        else if(std::abs(measured.rasterScale-scale)>.0001)c.failureItem=4;
        return false;
    }
    static void queueGeometry(const std::shared_ptr<Control>& state) noexcept {
        if(!state->active || state->geometryQueued)return;
        try {
            state->geometryQueued=true;
            state->root.Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low,[weak=state->self] {
                if(auto current=weak.lock()){current->geometryQueued=false;apply(*current);}
            });
        }catch(...){state->geometryQueued=false;fail(*state,winrt::to_hresult());}
    }
    bool bind(HWND hwnd,const taskbar::SurfaceSpec& surface) {
        wchar_t windowClass[64]{};
        if(!GetClassNameW(hwnd,windowClass,64) || (wcscmp(windowClass,L"Shell_TrayWnd") && wcscmp(windowClass,L"Shell_SecondaryTrayWnd")) ||
            (!surface.enabled && !standardPanel(hwnd)))return false;
        constexpr GUID ids[]{
            {0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}},
            {0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}},
            {0x2007f389,0x0b9d,0x4d16,{0xaf,0x78,0xba,0xfe,0x77,0x12,0x27,0x28}}};
        const wchar_t* names[]{L"Taskbar.dll",L"Taskbar.View.dll",L"SystemTray.dll"};
        for(unsigned i=0;i<3;++i)if(!native_layout::matches(GetModuleHandleW(names[i]),ids[i]))return false;
        const auto frame=context_menu::taskbarElement(hwnd);
        if(!frame || !frame.XamlRoot())return false;
        auto c=std::make_shared<Control>();c->panel=hwnd;c->surface=surface;c->root=frame.XamlRoot().Content().try_as<FrameworkElement>();
        if(!c->root || !c->root.Dispatcher().HasThreadAccess())return false;
        taskbar::Rectangle viewport;
        if(surface.enabled && !measureSurface(*c,viewport)){lastReport_.failurePhase=8;return false;}
        std::vector<DependencyObject> pending{c->root};unsigned visited=0;
        while(!pending.empty()) {
            if(++visited>512)return false;
            auto node=pending.back();pending.pop_back();
            if(auto element=node.try_as<FrameworkElement>()) {
                if(element.Name()==L"TaskbarFrameRepeater"){if(c->repeater)return false;c->repeater=element;}
                if(auto panel=element.try_as<Controls::StackPanel>();panel && panel.Name()==L"SystemTrayFrameGrid") {if(c->tray)return false;c->tray=panel;}
            }
            const int count=Media::VisualTreeHelper::GetChildrenCount(node);
            if(count<0 || pending.size()+count>512)return false;
            for(int i=0;i<count;++i)pending.push_back(Media::VisualTreeHelper::GetChild(node,i));
        }
        if(!c->repeater || !c->tray || c->tray.Orientation()!=Controls::Orientation::Horizontal)return false;
        Value original{nullptr};winrt::check_hresult(repeater::layout(c->repeater,original));
        if(!original || winrt::get_class_name(original)!=L"Taskbar.TaskbarCollapsibleLayout")return false;
        return install(c);
    }
    bool install(const std::shared_ptr<Control>& c) {
        winrt::check_hresult(repeater::layout(c->repeater,c->layout));
        winrt::check_hresult(repeater::itemsSource(c->repeater,c->source));
        if(!c->layout || !c->source)return false;
        // WinUI copies an IIterable-only source and does not track its events.
        // Notifications alone must not grant live-model capability.
        if(!repeater::sourceCapabilities(c->source).liveIndexed())return false;
        const std::weak_ptr<Control> weak=c;
        c->self=c;control_=c;pin();
        notifications_=c->source.try_as<Interop::INotifyCollectionChanged>();
        if(notifications_)changed_=notifications_.CollectionChanged([weak](auto&&,auto&&){if(auto state=weak.lock())++state->generation;});
        else {
            vector_=c->source.try_as<winrt::Windows::Foundation::Collections::IObservableVector<Value>>();
            if(!vector_)return false;
            changed_=vector_.VectorChanged([weak](auto&&,auto&&){if(auto state=weak.lock())++state->generation;});
        }
        modelSubscribed_=true;
        updated_=c->root.LayoutUpdated([weak](auto&&,auto&&){if(auto state=weak.lock()) {
            if(state->surface.enabled)queueGeometry(state);else apply(*state);
        }});subscribed_=true;
        traySize_=c->tray.SizeChanged([weak](auto&&,auto&&){if(auto state=weak.lock())++state->generation;});traySizeSubscribed_=true;
        return true;
    }
public:
    Session()=default;Session(const Session&)=delete;Session& operator=(const Session&)=delete;
    static HRESULT surfaceStatus(HWND hwnd,const taskbar::SurfaceSpec& surface,SurfaceGeometryReport* geometry=nullptr) noexcept {
        try {
            const auto frame=context_menu::taskbarElement(hwnd);
            if(!frame || !frame.XamlRoot())return E_ABORT;
            auto sample=std::make_unique<Control>();sample->panel=hwnd;sample->surface=surface;
            sample->root=frame.XamlRoot().Content().try_as<FrameworkElement>();
            if(!sample->root)return E_ABORT;
            sample->root.UpdateLayout();taskbar::Rectangle viewport;
            if(geometry) {
                RECT window{},client{};GetWindowRect(hwnd,&window);GetClientRect(hwnd,&client);
                const auto size=sample->root.XamlRoot().Size();
                geometry->rootWidth=sample->root.ActualWidth();geometry->rootHeight=sample->root.ActualHeight();
                geometry->xamlWidth=size.Width;geometry->xamlHeight=size.Height;
                geometry->frameWidth=frame.ActualWidth();geometry->frameHeight=frame.ActualHeight();
                geometry->windowWidth=window.right-window.left;geometry->windowHeight=window.bottom-window.top;
                geometry->clientWidth=client.right-client.left;geometry->clientHeight=client.bottom-client.top;
                geometry->dpi=GetDpiForWindow(hwnd);geometry->rasterScale=sample->root.XamlRoot().RasterizationScale();
            }
            return measureSurface(*sample,viewport)?S_OK:MAKE_HRESULT(SEVERITY_ERROR,FACILITY_ITF,0x300+sample->failureItem);
        }catch(...){return winrt::to_hresult();}
    }
    bool ownsClip() const noexcept {return control_ && control_->active && control_->clipFactory && !control_->confirmed.empty();}
    ~Session(){reset();}
    void notifyFailure(HWND owner,HWND target,unsigned serial) noexcept {
        if(control_ && control_->active){control_->notificationOwner=owner;control_->notificationTarget=target;control_->notificationSerial=serial;}
    }
    HRESULT configure(HWND hwnd,const ComponentLayoutRequest& request,taskbar::Rectangle viewport={},ClipFactory clipFactory={},taskbar::SurfaceSpec surface={}) noexcept {
        if(!validComponentLayout(request))return E_INVALIDARG;
        if(!request.count)return reset()?S_OK:E_ABORT;
        HWND interruptedOwner{},interruptedTarget{};unsigned interruptedSerial=0;
        try {
            if(!control_)lastReport_={};
            if(!control_ && !bind(hwnd,surface)){reset();return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);}
            if(!control_->active) {
                const auto status=FAILED(control_->status)?control_->status:E_ABORT;
                lastReport_=report(status);reset();return status;
            }
            const auto previousRequest=control_->request;
            const auto previousClipFactory=control_->clipFactory;
            const auto previousSurface=control_->surface;
            const auto previousViewport=control_->viewport;
            const auto previousRootWidth=control_->viewportRootWidth,previousRootHeight=control_->viewportRootHeight;
            const auto previousOwner=control_->notificationOwner;
            interruptedOwner=previousOwner;interruptedTarget=control_->notificationTarget;
            interruptedSerial=control_->notificationSerial;
            // An uncommitted candidate must not invalidate the previous serial.
            // Publish loss only if restoring that committed layout also fails.
            control_->notificationOwner=nullptr;
            control_->request=request;control_->viewport=viewport;
            control_->clipFactory=std::move(clipFactory);
            control_->surface=surface;
            control_->viewportRootWidth=control_->root.ActualWidth();control_->viewportRootHeight=control_->root.ActualHeight();
            ++control_->generation;control_->status=E_PENDING;control_->appliedGeneration=~0u;
            control_->windowStart=GetTickCount64();control_->windowAttempts=0;
            control_->tray.InvalidateArrange();control_->repeater.InvalidateMeasure();control_->root.UpdateLayout();
            apply(*control_);
            const auto status=control_->status;
            if(FAILED(status)) {
                // Preserve bounded diagnostics without retaining failed objects.
                lastReport_=report(status);
                if(previousRequest.count) {
                    control_->active=true;control_->request=previousRequest;control_->viewport=previousViewport;
                    control_->clipFactory=previousClipFactory;
                    control_->surface=previousSurface;
                    control_->viewportRootWidth=previousRootWidth;control_->viewportRootHeight=previousRootHeight;
                    ++control_->generation;control_->appliedGeneration=~0u;control_->status=E_PENDING;
                    control_->failurePhase=0;control_->failureItem=0;
                    control_->tray.InvalidateArrange();control_->repeater.InvalidateMeasure();control_->root.UpdateLayout();
                    apply(*control_);
                    if(SUCCEEDED(control_->status)) {
                        control_->notificationOwner=previousOwner;
                        return status; // Candidate refused; previous layout remains live.
                    }
                }
                if(previousOwner)PostMessageW(previousOwner,placementLostMessage,control_->notificationSerial,
                    reinterpret_cast<LPARAM>(control_->notificationTarget));
                reset();
            } else {
                control_->notificationOwner=previousOwner;
            }
            return status;
        }catch(...){
            const auto error=winrt::to_hresult();reset();
            if(interruptedOwner)PostMessageW(interruptedOwner,placementLostMessage,interruptedSerial,reinterpret_cast<LPARAM>(interruptedTarget));
            return error;
        }
    }
    bool reset() noexcept {
        if(!control_)return true;
        bool success=true;auto c=control_;c->active=false;
        try {if(subscribed_){c->root.LayoutUpdated(updated_);subscribed_=false;}}
        catch(...){success=false;}
        try {if(traySizeSubscribed_){c->tray.SizeChanged(traySize_);traySizeSubscribed_=false;}}
        catch(...){success=false;}
        try {
            if(modelSubscribed_) {
                if(notifications_)notifications_.CollectionChanged(changed_);
                else if(vector_)vector_.VectorChanged(changed_);
                modelSubscribed_=false;
            }
        }catch(...){success=false;}
        try {
            // Ask the unchanged owner to lay out its own objects again. Never
            // replace Layout or overwrite a source/view rebuilt by its owner.
            Value current{nullptr},source{nullptr};
            winrt::check_hresult(repeater::layout(c->repeater,current));
            winrt::check_hresult(repeater::itemsSource(c->repeater,source));
            if(current!=c->layout || source!=c->source)success=false;
            else {
                c->tray.InvalidateArrange();c->repeater.InvalidateArrange();c->root.InvalidateMeasure();c->root.UpdateLayout();
                if(c->clipFactory) {
                    std::vector<taskbar::Rectangle> original;
                    for(const auto& binding:c->bindings)if(binding.element.Visibility()==Visibility::Visible &&
                        Media::VisualTreeHelper::GetParent(binding.element)==binding.parent)
                        original.push_back(bounds(*c,binding.element));
                    auto clip=c->clipFactory(original,c->root.ActualWidth(),c->root.ActualHeight());
                    if(!clip || !clip->apply())success=false;
                }
            }
        }catch(...){success=false;}
        control_.reset();notifications_=nullptr;vector_=nullptr;
        return success;
    }
    ComponentLayoutReport report(HRESULT status) const noexcept {
        ComponentLayoutReport result;result.status=status;
        if(FAILED(status) && control_ && SUCCEEDED(control_->status) && lastReport_.status==status) {
            result=lastReport_;result.bound=1;return result;
        }
        if(control_){if(FAILED(control_->status))result.status=control_->status;result.bound=1;result.items=static_cast<unsigned>(control_->bindings.size());result.passes=control_->passes;result.failurePhase=control_->failurePhase;result.failureItem=control_->failureItem;}
        else if(FAILED(status)){result=lastReport_;result.status=status;result.bound=0;}
        return result;
    }
private:
    ComponentLayoutReport lastReport_;
};
}
