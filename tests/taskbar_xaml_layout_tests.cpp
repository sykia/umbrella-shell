// Private-process offscreen tool-window XAML island: no Explorer or global input.
#include <Windows.h>
#include <unknwn.h>
#include <cstring>
#undef GetCurrentTime
#include "platform/windows/explorer/TaskbarLayoutSlotAdapter.h"
#include "platform/windows/explorer/TaskbarRepeaterAccess.h"
#include "platform/windows/explorer/TaskbarRepeaterModel.h"
#include "platform/windows/explorer/TaskbarPanelOrder.h"
#include "fixtures/AttachedLayout.h"
#include "fixtures/ComponentLayout.h"
#include "platform/windows/explorer/TaskbarPlacementSurface.h"
#include "fixtures/NotifyCollectionSource.h"
#include "core/taskbar/TrayOrder.h"
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <iostream>
#include <stdexcept>
#include <memory>
using namespace winrt::Windows::UI::Xaml;
using namespace umbrella;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
namespace umbrella::explorer::component_layout {
struct ComponentLayoutFixture {
    static bool bind(Session& session,const FrameworkElement& root,const FrameworkElement& repeater,
        const Controls::StackPanel& tray,const Value& original,const Value& factory,
        winrt::com_ptr<layout_contract::Attached>& baseline,Value& expectedLayout,
        const std::vector<std::pair<UIElement,taskbar::Component>>& roles) {
        // The fixture owns this baseline layout. Session must only observe it.
        baseline=winrt::make_self<layout_contract::Attached>();baseline->original=original;
        auto realized=std::make_shared<std::vector<UIElement>>();
        baseline->measureCallback=[realized](const auto& context,auto desired) {
            *realized=layout_contract::elements(context);
            for(const auto& item:*realized)item.Measure({1000,48});
            return desired;
        };
        baseline->arrangeCallback=[realized](const auto&,auto size) {
            float x=0;
            for(const auto& item:*realized){item.Arrange({x,0,80,48});x+=80;}
            return size;
        };
        expectedLayout=baseline->create(factory);
        layout_contract::set(repeater,expectedLayout);
        root.InvalidateMeasure();root.UpdateLayout();
        auto control=std::make_shared<Session::Control>();control->root=root;control->repeater=repeater;control->tray=tray;
        for(const auto& [item,role]:roles)control->roles.push_back({item,role});
        return session.install(control);
    }
    static unsigned generation(const Session& session){return session.control_->generation;}
    static void usePanel(Session& session,HWND panel){session.control_->panel=panel;}
    static bool budgetPending(const Session& session){return session.control_ && session.control_->budgetResetQueued;}
    static void settle(const Session& session) {
        const auto deadline=GetTickCount64()+1000;
        do {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
            if(!budgetPending(session))return;
            MsgWaitForMultipleObjectsEx(0,nullptr,10,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        }while(GetTickCount64()<deadline);
        require(false,"Private dispatcher did not finish the pending budget callback");
    }
    static double expectedX(const Session& session,const UIElement& element) {
        for(const auto& move:session.control_->confirmed)if(session.control_->bindings[move.instance-1].element==element)return move.after.x;
        winrt::throw_hresult(E_UNEXPECTED);
    }
    static void addRole(Session& session,const UIElement& item,taskbar::Component role) {
        session.control_->roles.push_back({item,role});
    }
};
}
void exerciseLayoutRequest() {
    using namespace explorer;
    require(repeater::sourceCapabilities(nullptr).status==E_INVALIDARG,"Null source query claimed success");
    ComponentLayoutRequest request;
    require(validComponentLayout(request),"Owned fixture reset rejected");
    request.flags=1;
    require(!validComponentLayout(request),"Retired delegated-layout probe accepted");
    request.flags=2;
    require(!validComponentLayout(request),"Unknown fixture operation accepted");
    request={};request.count=1;wcscpy_s(request.entries[0].id,L"start");
    require(validComponentLayout(request),"Known fixture selector rejected");
    request.entries[0].zone=3;
    require(!validComponentLayout(request),"Unknown fixture zone accepted");
    request.entries[0].zone=0;request.count=2;request.entries[1]=request.entries[0];
    require(!validComponentLayout(request),"Duplicate fixture selector accepted");
    request.count=1;std::fill(std::begin(request.entries[0].id),std::end(request.entries[0].id),L'x');
    require(!validComponentLayout(request),"Unterminated fixture selector accepted");
    request={};request.count=maxLayoutGroups+1;
    require(!validComponentLayout(request),"Out-of-bounds fixture array accepted");
    request={};request.gap=97;
    require(!validComponentLayout(request),"Unbounded fixture spacing accepted");
    request={};request.version=2;
    require(!validComponentLayout(request),"Unknown fixture schema accepted");
    for(unsigned i=1;i<=static_cast<unsigned>(taskbar::Component::SystemIndicators);++i) {
        const auto role=static_cast<taskbar::Component>(i);
        require(taskbar::componentFromId(taskbar::stableId(role))==role,"Registry identity round trip failed");
    }
    require(taskbar::componentFromId(L"invalid")==taskbar::Component::Unknown,"Unknown registry selector accepted");
    require(taskbar::identify(L"SystemTray.Stack",L"MainStack",L"")==taskbar::Component::SystemIndicators,"System indicators registry mismatch");
}
void exerciseRepeaterContract() {
    wchar_t systemDirectory[MAX_PATH]{};
    require(GetWindowsDirectoryW(systemDirectory,MAX_PATH)>0,"Windows directory unavailable");
    const auto path = std::wstring(systemDirectory)+L"\\SystemApps\\Microsoft.UI.Xaml.CBS_8wekyb3d8bbwe\\Microsoft.UI.Xaml.dll";
    const HMODULE module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(module!=nullptr,"System WinUI 2 CBS runtime unavailable");
    using GetFactory=HRESULT(WINAPI*)(void*,void**);
    const auto getFactory=reinterpret_cast<GetFactory>(GetProcAddress(module,"DllGetActivationFactory"));
    require(getFactory!=nullptr,"WinUI 2 activation factory unavailable");
    winrt::Windows::Foundation::IActivationFactory factory{nullptr};
    const winrt::hstring name=L"Microsoft.UI.Xaml.Controls.ItemsRepeater";
    winrt::check_hresult(getFactory(winrt::get_abi(name),winrt::put_abi(factory)));
    constexpr GUID repeaterFactoryId{0xc3c1f244,0x67a8,0x568f,{0xa6,0xf7,0x5d,0xa8,0xb0,0xea,0xdd,0x49}};
    winrt::com_ptr<::IInspectable> creating;
    winrt::check_hresult(reinterpret_cast<::IUnknown*>(winrt::get_abi(factory))->QueryInterface(repeaterFactoryId,creating.put_void()));
    using Create=HRESULT(STDMETHODCALLTYPE*)(void*,void*,void**,void**);
    auto** factoryTable=*reinterpret_cast<void***>(creating.get());
    explorer::repeater::Value instance{nullptr},inner{nullptr};
    winrt::check_hresult(reinterpret_cast<Create>(factoryTable[6])(creating.get(),nullptr,winrt::put_abi(inner),winrt::put_abi(instance)));
    require(instance.try_as<FrameworkElement>()!=nullptr,"Activated repeater is not Windows.UI.Xaml");
    explorer::repeater::Value layout{nullptr};
    winrt::check_hresult(explorer::repeater::layout(instance,layout));
    require(layout!=nullptr,"Actual ItemsRepeater layout unavailable");
    Controls::Button unrelated;
    int index=123;
    winrt::check_hresult(explorer::repeater::elementIndex(instance,unrelated,index));
    require(index==-1,"Unrelated element received a repeater model index");
    require(explorer::repeater::layout(unrelated,layout)==E_NOINTERFACE && !layout,"Plain button granted repeater layout access");
    index=123;
    require(explorer::repeater::elementIndex(unrelated,unrelated,index)==E_NOINTERFACE && index==-1,"Unsupported object granted a model index");
    require(explorer::repeater::layout(nullptr,layout)==E_INVALIDARG && !layout,"Null repeater accepted");
    // This fixture owns its source. Never perform this mutation on Explorer's
    // foreign source merely because its interface happens to be writable.
    auto data=winrt::single_threaded_observable_vector<explorer::repeater::Value>();
    const auto vectorCapabilities=explorer::repeater::sourceCapabilities(data);
    require(vectorCapabilities.liveIndexed() && (vectorCapabilities.contracts&explorer::SourceObservableVector),"Observable source contracts not detected");
    const std::array<Controls::Button,3> originals{};
    std::array<unsigned,3> nativeClicks{};
    for (unsigned i=0;i<originals.size();++i) {
        const auto& button=originals[i];
        button.Width(44); button.Height(48);
        button.Click([&,i](auto&&,auto&&){++nativeClicks[i];});
        data.Append(button);
    }
    winrt::com_ptr<::IInspectable> contract;
    winrt::check_hresult(reinterpret_cast<::IUnknown*>(winrt::get_abi(instance))->QueryInterface(explorer::repeater::itemsRepeaterId,contract.put_void()));
    auto** table=*reinterpret_cast<void***>(contract.get());
    using PutSource=HRESULT(STDMETHODCALLTYPE*)(void*,void*);
    winrt::check_hresult(reinterpret_cast<PutSource>(table[7])(contract.get(),winrt::get_abi(data)));
    struct Window {
        HWND handle=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"",WS_POPUP,-32000,-32000,1000,160,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ~Window(){ if(handle) DestroyWindow(handle); }
    } window;
    require(window.handle!=nullptr,"Repeater fixture HWND failed");
    Hosting::DesktopWindowXamlSource source;
    auto host=source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(host->AttachToWindow(window.handle));
    HWND child{}; winrt::check_hresult(host->get_WindowHandle(&child));
    require(SetWindowPos(child,nullptr,0,0,1000,160,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"Repeater fixture sizing failed");
    auto view=instance.as<FrameworkElement>(); source.Content(view);
    ShowWindow(child,SW_SHOWNOACTIVATE); ShowWindow(window.handle,SW_SHOWNOACTIVATE);
    const auto flush=[&] {
        view.InvalidateMeasure(); view.Measure({1000,160}); view.Arrange({0,0,1000,160}); view.UpdateLayout();
        MSG message{}; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    };
    using Realize=HRESULT(STDMETHODCALLTYPE*)(void*,int,void**);
    for(int i=0;i<3;++i) {
        explorer::repeater::Value realized{nullptr};
        winrt::check_hresult(reinterpret_cast<Realize>(table[21])(contract.get(),i,winrt::put_abi(realized)));
        require(realized==originals[i],"ItemsRepeater replaced original UIElement");
    }
    flush();
    const auto verify=[&](const std::array<int,3>& expected) {
        for(int i=0;i<3;++i) {
            // The offscreen viewport may legitimately recycle every element.
            // Explicitly realize only these three fixture-owned data objects.
            explorer::repeater::Value realized{nullptr};
            winrt::check_hresult(reinterpret_cast<Realize>(table[21])(contract.get(),expected[i],winrt::put_abi(realized)));
            require(realized==originals[i],"Native reordered source replaced an original object");
            int actual=-1; winrt::check_hresult(explorer::repeater::elementIndex(instance,originals[i],actual));
            if(actual!=expected[i]) std::cerr << "model object=" << i << " actual=" << actual << " expected=" << expected[i] << '\n';
            require(actual==expected[i],"Native repeater model index does not match confirmed order");
        }
    };
    verify({0,1,2});
    explorer::repeater::Owner resolved;
    winrt::check_hresult(explorer::repeater::owner(originals[0],resolved));
    require(resolved.container==instance && resolved.element==originals[0] && resolved.index==0 && resolved.depth==0,"Direct native model owner not resolved");
    require(explorer::repeater::owner(nullptr,resolved)==E_INVALIDARG && !resolved.element && !resolved.container,"Null owner lookup retained a stale binding");
    const std::vector<explorer::repeater::Value> before{originals[0],originals[1],originals[2]};
    const std::vector<explorer::repeater::Value> after{originals[2],originals[0],originals[1]};
    const auto nativeVerify=[&](const auto& order) {
        flush();
        for(unsigned i=0;i<order.size();++i) {
            explorer::repeater::Value realized{nullptr};
            winrt::check_hresult(reinterpret_cast<Realize>(table[21])(contract.get(),i,winrt::put_abi(realized)));
            int actual=-1; winrt::check_hresult(explorer::repeater::elementIndex(instance,realized,actual));
            if (realized!=order[i] || actual!=static_cast<int>(i)) return false;
        }
        return true;
    };
    using taskbar::Transaction;
    require(explorer::repeater::reorderOwnedModel(instance,data,before,after,nativeVerify)==Transaction::Applied,"Native model transaction failed");
    verify({1,2,0});
    for (const auto& button : originals) {
        auto peer=Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(button);
        peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    }
    flush();
    require(nativeClicks==std::array<unsigned,3>{1,1,1},"Reordered native button lost original Click handler");
    require(explorer::repeater::reorderOwnedModel(instance,data,after,before,nativeVerify)==Transaction::Applied,"Native model restore failed");
    verify({0,1,2});
    require(explorer::repeater::reorderOwnedModel(instance,data,before,after,[&](const auto& order){return order==before && nativeVerify(order);})==Transaction::RolledBack,"Native verification refusal did not rollback");
    verify({0,1,2});
    Controls::Button arriving; arriving.Width(44); arriving.Height(48); data.Append(arriving);
    require(explorer::repeater::reorderOwnedModel(instance,data,before,after,nativeVerify)==Transaction::Stale,"Arriving application ignored stale model");
    auto expanded=before; expanded.push_back(arriving);
    auto expandedOrder=expanded; std::rotate(expandedOrder.begin(),expandedOrder.begin()+3,expandedOrder.end());
    require(explorer::repeater::reorderOwnedModel(instance,data,expanded,expandedOrder,nativeVerify)==Transaction::Applied,"Expanded native model reorder failed");
    require(explorer::repeater::reorderOwnedModel(instance,data,expandedOrder,expanded,nativeVerify)==Transaction::Applied,"Expanded native model restore failed");
    data.RemoveAt(3);
    require(explorer::repeater::reorderOwnedModel(instance,data,expanded,expandedOrder,nativeVerify)==Transaction::Stale,"Closed application ignored stale model");
    require(nativeVerify(before),"Dynamic model restore lost original identities");
    bool injected=false;
    const auto token=data.VectorChanged([&](auto&&,auto&&){if(!injected){injected=true;data.Append(arriving);}});
    const auto interrupted=explorer::repeater::reorderOwnedModel(instance,data,before,after,nativeVerify);
    data.VectorChanged(token);
    require(interrupted==Transaction::RollbackFailed && data.GetAt(data.Size()-1)==arriving,"Reentrant model update was overwritten");
    // Fixture explicitly owns cleanup; transaction correctly refused to clobber
    // a changed model, so this is not reported as transactional rollback.
    data.Clear(); for(const auto& item : before) data.Append(item);
    require(nativeVerify(before),"Owned fixture cleanup failed");
    data.Clear();
    Controls::Button nestedButton;
    Controls::TextBlock glyph; glyph.Text(L"indicator");
    Controls::StackPanel content;
    content.Children().Append(nestedButton); content.Children().Append(glyph);
    Controls::Border wrapper; wrapper.Child(content); wrapper.Width(80); wrapper.Height(48);
    data.Append(wrapper); data.Append(arriving);
    const std::vector<explorer::repeater::Value> nestedBefore{wrapper,arriving}, nestedAfter{arriving,wrapper};
    require(nativeVerify(nestedBefore),"Nested native fixture did not realize");
    winrt::check_hresult(explorer::repeater::owner(nestedButton,resolved));
    require(resolved.container==instance && resolved.element==wrapper && resolved.index==0 && resolved.depth==2,"Nested button was confused with its owning model item");
    explorer::repeater::Owner indicatorOwner;
    winrt::check_hresult(explorer::repeater::owner(glyph,indicatorOwner));
    require(indicatorOwner.element==resolved.element && indicatorOwner.index==resolved.index,"Indicator and original button assigned different model owners");
    require(explorer::repeater::reorderOwnedModel(instance,data,nestedBefore,nestedAfter,nativeVerify)==Transaction::Applied,"Wrapped native component could not reorder as one object");
    winrt::check_hresult(explorer::repeater::owner(nestedButton,resolved));
    require(resolved.element==wrapper && resolved.index==1 && wrapper.Child()==content && content.Children().GetAt(0)==nestedButton && content.Children().GetAt(1)==glyph,"Wrapped reorder detached original content or retained stale index");
    data.Clear();
    explorer::repeater::Value nestedRepeater{nullptr}, nestedInner{nullptr};
    winrt::check_hresult(reinterpret_cast<Create>(factoryTable[6])(creating.get(),nullptr,winrt::put_abi(nestedInner),winrt::put_abi(nestedRepeater)));
    winrt::com_ptr<::IInspectable> nestedContract;
    winrt::check_hresult(reinterpret_cast<::IUnknown*>(winrt::get_abi(nestedRepeater))->QueryInterface(explorer::repeater::itemsRepeaterId,nestedContract.put_void()));
    auto** nestedTable=*reinterpret_cast<void***>(nestedContract.get());
    auto nestedData=winrt::single_threaded_observable_vector<explorer::repeater::Value>();
    Controls::Button innerButton; nestedData.Append(innerButton);
    winrt::check_hresult(reinterpret_cast<PutSource>(nestedTable[7])(nestedContract.get(),winrt::get_abi(nestedData)));
    Controls::Border shell; shell.Child(nestedRepeater.as<FrameworkElement>()); data.Append(shell);
    require(nativeVerify(std::vector<explorer::repeater::Value>{shell}),"Outer repeater did not realize nested model");
    explorer::repeater::Value innerRealized{nullptr};
    winrt::check_hresult(reinterpret_cast<Realize>(nestedTable[21])(nestedContract.get(),0,winrt::put_abi(innerRealized)));
    winrt::check_hresult(explorer::repeater::owner(innerButton,resolved));
    require(resolved.container==nestedRepeater && resolved.element==innerButton && resolved.index==0,"Nested repeater boundary crossed into outer model");
    data.Clear();
    Controls::Button deepButton;
    UIElement deep=deepButton;
    for(unsigned i=0;i<9;++i) { Controls::Border layer; layer.Child(deep); deep=layer; }
    data.Append(deep);
    require(nativeVerify(std::vector<explorer::repeater::Value>{deep}),"Deep native fixture did not realize");
    require(explorer::repeater::owner(deepButton,resolved)==E_BOUNDS && !resolved.element && !resolved.container,"Unbounded ancestry or stale owner accepted");
    Controls::Button detached;
    require(FAILED(explorer::repeater::owner(detached,resolved)) && !resolved.element,"Detached element acquired a native owner");
    data.Clear();
    Controls::Button attachedFirst,attachedSecond,attachedThird;
    for(auto button:{attachedFirst,attachedSecond,attachedThird}) {button.Width(80);button.Height(48);data.Append(button);}
    explorer::repeater::Value priorLayout{nullptr};
    winrt::check_hresult(explorer::repeater::layout(instance,priorLayout));
    auto attached=winrt::make_self<explorer::layout_contract::Attached>();
    attached->original=priorLayout;
    auto customFactoryName=winrt::hstring(L"Microsoft.UI.Xaml.Controls.VirtualizingLayout");
    winrt::Windows::Foundation::IActivationFactory customFactory{nullptr};
    winrt::check_hresult(getFactory(winrt::get_abi(customFactoryName),winrt::put_abi(customFactory)));
    auto custom=attached->create(customFactory);
    attached->measureCallback=[](const auto& context,auto desired) {
        // This private HWND is outside the effective viewport. Retrieve owned
        // items during Measure, never from Arrange (which causes a recycle/
        // realize layout cycle for an empty virtualization viewport).
        for(const auto& item:explorer::layout_contract::elements(context))item.Measure({1000,160});
        return desired;
    };
    attached->arrangeCallback=[&](const auto&,auto finalSize) {
        auto items=data.GetView();
        for(unsigned i=0;i<items.Size();++i)items.GetAt(i).as<UIElement>().Arrange({static_cast<float>((items.Size()-i-1)*80),0,80,48});
        return finalSize;
    };
    explorer::layout_contract::set(instance,custom);
    try {flush();}catch(...) {
        std::cerr<<"attached passes="<<attached->measures<<","<<attached->arranges<<","<<attached->changes<<" failure="<<std::hex<<attached->failure<<"\n";
        throw;
    }
    require(attached->measures>0 && attached->arranges>0,"Custom public layout overrides were not called");
    auto attachedX=[&](const auto& element){return element.TransformToVisual(view).TransformPoint({0,0}).X;};
    require(attachedX(attachedFirst)>attachedX(attachedSecond) && attachedX(attachedSecond)>attachedX(attachedThird),"Delegated attached layout did not own the final real slots");
    require(attachedFirst.Translation()==winrt::Windows::Foundation::Numerics::float3{} && Media::VisualTreeHelper::GetParent(attachedFirst)==view,"Attached layout changed parent or used Translation");
    const auto beforeInvalidation=attached->measures;
    explorer::layout_contract::invalidate(priorLayout,true);view.UpdateLayout();
    require(attached->measures>beforeInvalidation,"Original layout invalidation did not reach its replacement");
    const auto beforeChange=attached->changes;
    Controls::Button attachedNew;attachedNew.Width(80);attachedNew.Height(48);data.Append(attachedNew);
    flush();
    require(attached->changes>beforeChange && attachedX(attachedFirst)>attachedX(attachedNew),"Dynamic model change did not reach original/custom layout");
    explorer::layout_contract::set(instance,priorLayout);
    flush();
    explorer::repeater::Value restoredLayout{nullptr};
    winrt::check_hresult(explorer::repeater::layout(instance,restoredLayout));
    require(restoredLayout==priorLayout,"Original attached layout was not restored");
    std::cout<<"Passed: delegated public WinUI 2 layout callbacks, real slot ordering, original model/parents and dynamic notifications, then original layout restore.\n";
    constexpr GUID stackId{0x6bf7f61b,0x0cef,0x5d92,{0xa8,0x28,0x93,0xef,0xbd,0x6d,0xf4,0xc1}};
    auto stackContract=explorer::layout_contract::contract(priorLayout,stackId);
    using Orientation=HRESULT(STDMETHODCALLTYPE*)(void*,int);
    winrt::check_hresult(reinterpret_cast<Orientation>((*reinterpret_cast<void***>(stackContract.get()))[7])(stackContract.get(),0));
    source.Content(nullptr);
    Controls::Grid commonRoot;commonRoot.Width(1000);commonRoot.Height(48);
    Controls::StackPanel tray;tray.Orientation(Controls::Orientation::Horizontal);tray.Width(176);tray.Height(48);tray.HorizontalAlignment(HorizontalAlignment::Right);
    Controls::Button overflow,input,quick,clock;
    overflow.Width(32);input.Width(44);quick.Width(60);clock.Width(40);
    for(auto button:{overflow,input,quick,clock}){button.Height(48);tray.Children().Append(button);}
    view.Height(48);commonRoot.Children().Append(view);commonRoot.Children().Append(tray);source.Content(commonRoot);
    commonRoot.Measure({1000,48});commonRoot.Arrange({0,0,1000,48});commonRoot.UpdateLayout();
    explorer::component_layout::Session coordinated;
    winrt::com_ptr<explorer::layout_contract::Attached> baseline;
    explorer::repeater::Value observedLayout{nullptr};
    require(explorer::component_layout::ComponentLayoutFixture::bind(coordinated,commonRoot,view,tray,priorLayout,customFactory,baseline,observedLayout,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},{input,taskbar::Component::InputIndicators},
         {quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),"Shared-root native coordinator fixture did not bind");
    const auto checkLayoutIdentity=[&] {
        explorer::repeater::Value current{nullptr};
        winrt::check_hresult(explorer::repeater::layout(instance,current));
        require(current==observedLayout,"Coordinator replaced the owner's original Layout");
    };
    checkLayoutIdentity();
    explorer::ComponentLayoutRequest coordinatedRequest;
    const std::array<const wchar_t*,8> names{L"start",L"overflow",L"search",L"apps",L"quicksettings",L"taskview",L"input",L"clock"};
    const std::array<unsigned,8> zones{0,0,1,1,1,2,2,2};coordinatedRequest.count=8;
    for(unsigned i=0;i<8;++i){wcscpy_s(coordinatedRequest.entries[i].id,names[i]);coordinatedRequest.entries[i].zone=zones[i];}
    const auto coordinatedStatus=coordinated.configure(nullptr,coordinatedRequest);
    if(FAILED(coordinatedStatus))std::cerr<<"coordinator HRESULT="<<std::hex<<coordinatedStatus<<" phase="<<coordinated.report(coordinatedStatus).failurePhase<<" item="<<coordinated.report(coordinatedStatus).failureItem<<'\n';
    checkLayoutIdentity();
    require(SUCCEEDED(coordinatedStatus),"Shared-root actual central/tray cross-zone layout or original XAML hit-testing refused");
    require(quick.TransformToVisual(commonRoot).TransformPoint({0,0}).X<700 && overflow.TransformToVisual(commonRoot).TransformPoint({0,0}).X<200,
        "Original tray controls did not leave the right-hand parent bounds");
    require(Media::VisualTreeHelper::GetParent(quick)==tray && Media::VisualTreeHelper::GetParent(attachedSecond)==view,"Cross-zone placement reparented original controls");
    require(coordinated.report(S_OK).passes<8,"Stable cross-zone layout repeated without a native invalidation");
    // The same native objects and original Layout survive geometry changes.
    // HWND remains offscreen; neither Explorer nor global input is touched.
    const auto geometryTurn=[] {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    };
    for (const double thickness : {64.,96.,48.}) {
        geometryTurn(); // Do not override the child's owner-controlled size.
        commonRoot.Height(thickness);commonRoot.Width(900);
        commonRoot.Measure({900,static_cast<float>(thickness)});
        commonRoot.Arrange({0,0,900,static_cast<float>(thickness)});commonRoot.UpdateLayout();
        const auto geometryStatus=coordinated.configure(nullptr,coordinatedRequest,{16,0,868,thickness});
        if(FAILED(geometryStatus))std::cerr<<"geometry="<<thickness<<" phase="<<coordinated.report(geometryStatus).failurePhase
            <<" item="<<coordinated.report(geometryStatus).failureItem<<" status="<<std::hex<<geometryStatus<<std::dec<<'\n';
        require(SUCCEEDED(geometryStatus),
            "Measured native slots failed after width/thickness change");
        require(std::abs(explorer::component_layout::ComponentLayoutFixture::expectedX(coordinated,attachedFirst)-16)<.5,
            "Native left zone ignored the measured viewport inset");
        const auto bounds=quick.TransformToVisual(commonRoot).TransformBounds({0,0,
            static_cast<float>(quick.ActualWidth()),static_cast<float>(quick.ActualHeight())});
        require(bounds.X>=0 && bounds.X+bounds.Width<=900.5 && bounds.Y>=0 && bounds.Y+bounds.Height<=thickness+.5,
            "Native move escaped resized root");
        require(quick.Height()==48 && Media::VisualTreeHelper::GetParent(quick)==tray,
            "Thickness change resized/reparented a system control");
        checkLayoutIdentity();
        const auto refused=coordinated.configure(nullptr,coordinatedRequest,{16,0,100,thickness});
        require(FAILED(refused),
            "Too-small native viewport was committed");
        require(coordinated.report(refused).failurePhase==1,
            "Successful rollback hid the rejected candidate's diagnostics");
        require(SUCCEEDED(coordinated.report(S_OK).status) &&
            std::abs(explorer::component_layout::ComponentLayoutFixture::expectedX(coordinated,attachedFirst)-16)<.5,
            "Refused geometry did not restore the last confirmed native viewport");
    }
    geometryTurn();
    commonRoot.Width(1000);commonRoot.Height(48);
    commonRoot.Measure({1000,48});commonRoot.Arrange({0,0,1000,48});commonRoot.UpdateLayout();
    require(SUCCEEDED(coordinated.configure(nullptr,coordinatedRequest)),"Native baseline did not recover after geometry fixture");
    coordinatedRequest.entries[0].zone=2;coordinatedRequest.entries[3].zone=2;
    coordinatedRequest.entries[4].zone=0;coordinatedRequest.entries[7].zone=0;
    const auto originalRender=attachedFirst.RenderTransform();Media::TranslateTransform inFlight;
    inFlight.X(5);attachedFirst.RenderTransform(inFlight);
    require(SUCCEEDED(coordinated.configure(nullptr,coordinatedRequest)),"Second cross-zone native layout failed");
    const auto expectedX=explorer::component_layout::ComponentLayoutFixture::expectedX(coordinated,attachedFirst);
    attachedFirst.RenderTransform(originalRender);
    require(std::abs(attachedFirst.TransformToVisual(commonRoot).TransformPoint({0,0}).X-expectedX)<.5,"In-flight animation was persisted as a layout-slot offset");
    explorer::component_layout::ComponentLayoutFixture::settle(coordinated);
    Controls::Button newApp;newApp.Width(80);newApp.Height(48);
    explorer::component_layout::ComponentLayoutFixture::addRole(coordinated,newApp,taskbar::Component::Applications);
    const auto beforeModelChange=explorer::component_layout::ComponentLayoutFixture::generation(coordinated);
    data.Append(newApp);
    require(explorer::component_layout::ComponentLayoutFixture::generation(coordinated)>beforeModelChange,"Source event did not invalidate the layout cache");
    commonRoot.UpdateLayout();
    require(Media::VisualTreeHelper::GetParent(newApp)==view && coordinated.report(S_OK).items==9 &&
        SUCCEEDED(coordinated.report(S_OK).status),"New native application did not join the active coordinated model");
    checkLayoutIdentity();
    require(coordinated.reset(),"Geometry scenario did not release its original slots");
    // Exercise the production mask writer, with real HRGN and composition clip,
    // on this private HWND. A failing writer mutates before reporting failure.
    explorer::component_layout::Session surfaceCoordinated;
    explorer::component_layout::ComponentLayoutFixture::settle(surfaceCoordinated);
    // Measure a genuine monitor-anchored surface while this owned HWND is hidden.
    ShowWindow(window.handle,SW_HIDE);
    MONITORINFO surfaceMonitor{sizeof(surfaceMonitor)};
    require(GetMonitorInfoW(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY),&surfaceMonitor)!=FALSE,"Fixture monitor unavailable");
    const auto surfaceDpi=GetDpiForWindow(window.handle);
    const auto surfaceWidth=MulDiv(surfaceMonitor.rcMonitor.right-surfaceMonitor.rcMonitor.left,80,100);
    const auto surfaceHeight=MulDiv(64,static_cast<int>(surfaceDpi),96);
    const auto surfaceX=surfaceMonitor.rcMonitor.left+(surfaceMonitor.rcMonitor.right-surfaceMonitor.rcMonitor.left-surfaceWidth)/2;
    require(SetWindowPos(window.handle,nullptr,surfaceX,surfaceMonitor.rcMonitor.bottom-surfaceHeight,surfaceWidth,surfaceHeight,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE &&
        SetWindowPos(child,nullptr,0,0,surfaceWidth,surfaceHeight,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"Mask fixture sizing failed");
    commonRoot.Width(surfaceWidth*96./surfaceDpi);commonRoot.Height(64);
    geometryTurn();commonRoot.UpdateLayout();
    explorer::smooth_clip::Session surfaceSmooth;
    require(surfaceSmooth.bind(commonRoot),"Fixture composition surface did not bind");
    explorer::IslandMask surfaceMask{};bool surfaceApplied=false;
    GetWindowRect(window.handle,&surfaceMask.bounds);surfaceMask.count=1;
    surfaceMask.rectangles[0]={surfaceWidth*3/5,0,surfaceWidth,surfaceHeight};
    require(explorer::placement_surface::applyPacket(window.handle,surfaceSmooth,surfaceMask,surfaceApplied,surfaceMask),
        "Initial production island mask failed");
    require(explorer::component_layout::ComponentLayoutFixture::bind(surfaceCoordinated,commonRoot,view,tray,priorLayout,customFactory,baseline,observedLayout,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{newApp,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},
         {input,taskbar::Component::InputIndicators},{quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),
        "Independent clipping coordinator did not bind");
    explorer::component_layout::ComponentLayoutFixture::usePanel(surfaceCoordinated,window.handle);
    const explorer::component_layout::ClipFactory surfaceFactory=[&](auto boxes,double width,double height) {
        return explorer::placement_surface::prepare(window.handle,surfaceSmooth,surfaceMask,surfaceApplied,boxes,width,height);
    };
    explorer::component_layout::ComponentLayoutFixture::settle(surfaceCoordinated);
    const taskbar::SurfaceSpec ownedSurface{true,3,80,64,16};
    const auto surfaceStatus=surfaceCoordinated.configure(nullptr,coordinatedRequest,{16,0,968,48},surfaceFactory,ownedSurface);
    if(FAILED(surfaceStatus))std::cerr<<"surface status="<<std::hex<<surfaceStatus<<std::dec<<" phase="<<surfaceCoordinated.report(surfaceStatus).failurePhase
        <<" item="<<surfaceCoordinated.report(surfaceStatus).failureItem<<" root="<<commonRoot.ActualWidth()<<','<<commonRoot.ActualHeight()<<'\n';
    require(SUCCEEDED(surfaceStatus) && surfaceCoordinated.ownsClip(),
        "Native placement did not update stale clipping before hit-test verification");
    require(surfaceMask.count==3,"Contiguous controls did not produce three coherent destination islands");
    const auto committedClip=surfaceSmooth.snapshot();
    surfaceSmooth.reset();
    require(surfaceSmooth.restore(committedClip) && surfaceSmooth.snapshot().clip==committedClip.clip,
        "Composition rollback lost its previous clip after a writer reset");
    HRGN committedRegion=CreateRectRgn(0,0,0,0),actualRegion=CreateRectRgn(0,0,0,0);
    require(committedRegion && actualRegion && GetWindowRgn(window.handle,committedRegion)!=ERROR,"Committed input mask unavailable");
    const auto committedMask=surfaceMask;
    const auto committedX=explorer::component_layout::ComponentLayoutFixture::expectedX(surfaceCoordinated,attachedFirst);
    unsigned originalClicks=0;
    const auto clickToken=attachedFirst.Click([&](auto&&,auto&&){++originalClicks;});
    auto movedPeer=Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(attachedFirst);
    movedPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    geometryTurn();require(originalClicks==1,"Clipped original control lost Click");
    struct FailMask final:explorer::component_layout::ClipUpdate {
        std::unique_ptr<explorer::component_layout::ClipUpdate> inner;
        explicit FailMask(std::unique_ptr<explorer::component_layout::ClipUpdate> value):inner(std::move(value)){}
        bool apply() noexcept override {inner->apply();return false;}
        bool rollback() noexcept override {return inner->rollback();}
    };
    const explorer::component_layout::ClipFactory failingSurface=[&](auto boxes,double width,double height)->std::unique_ptr<explorer::component_layout::ClipUpdate> {
        auto update=surfaceFactory(boxes,width,height);
        return update?std::make_unique<FailMask>(std::move(update)):nullptr;
    };
    explorer::component_layout::ComponentLayoutFixture::settle(surfaceCoordinated);
    auto rejectedRequest=coordinatedRequest;rejectedRequest.entries[0].zone=0;
    const auto maskFailure=surfaceCoordinated.configure(nullptr,rejectedRequest,{24,0,952,48},failingSurface,ownedSurface);
    require(FAILED(maskFailure) && surfaceCoordinated.report(maskFailure).failurePhase==7 && SUCCEEDED(surfaceCoordinated.report(S_OK).status),
        "Partial mask failure was committed or previous slots were lost");
    require(GetWindowRgn(window.handle,actualRegion)!=ERROR && EqualRgn(actualRegion,committedRegion) &&
        !std::memcmp(&surfaceMask,&committedMask,sizeof(surfaceMask)),"Mask rollback did not restore committed input geometry");
    require(std::abs(attachedFirst.TransformToVisual(commonRoot).TransformPoint({0,0}).X-committedX)<.5,
        "Mask refusal did not restore the original component's slot");
    movedPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    geometryTurn();require(originalClicks==2,"Rollback broke the original control Click");
    attachedFirst.Click(clickToken);DeleteObject(actualRegion);DeleteObject(committedRegion);
    require(surfaceCoordinated.reset(),"Original public layout and tray slots did not restore");
    require(!surfaceCoordinated.ownsClip(),"Reset retained placement mask ownership");
    surfaceSmooth.reset();SetWindowRgn(window.handle,nullptr,TRUE);
    SetWindowPos(window.handle,nullptr,-32000,-32000,1000,160,SWP_NOACTIVATE|SWP_NOZORDER);
    SetWindowPos(child,nullptr,0,0,1000,160,SWP_NOACTIVATE|SWP_NOZORDER);
    commonRoot.Width(1000);commonRoot.Height(48);commonRoot.UpdateLayout();
    ShowWindow(window.handle,SW_SHOWNOACTIVATE);
    checkLayoutIdentity();
    require(quick.TransformToVisual(commonRoot).TransformPoint({0,0}).X>800 && overflow.TransformToVisual(commonRoot).TransformPoint({0,0}).X>800,"Tray slots remained outside their original region after reset");
    // A stale request must not replace a newer owner's source/layout in reset.
    explorer::component_layout::Session staleOwner;
    winrt::com_ptr<explorer::layout_contract::Attached> staleBaseline;
    explorer::repeater::Value staleExpected{nullptr};
    require(explorer::component_layout::ComponentLayoutFixture::bind(staleOwner,commonRoot,view,tray,priorLayout,customFactory,staleBaseline,staleExpected,{}),"Stale-owner fixture did not bind");
    auto newerData=winrt::single_threaded_observable_vector<explorer::repeater::Value>();
    Controls::Button newerButton;newerButton.Width(80);newerButton.Height(48);newerData.Append(newerButton);
    winrt::check_hresult(reinterpret_cast<PutSource>(table[7])(contract.get(),winrt::get_abi(newerData)));
    require(FAILED(staleOwner.configure(nullptr,coordinatedRequest)),"Rebuilt source accepted a stale cross-zone request");
    explorer::repeater::Value retainedSource{nullptr},retainedLayout{nullptr};
    winrt::check_hresult(explorer::repeater::itemsSource(instance,retainedSource));
    winrt::check_hresult(explorer::repeater::layout(instance,retainedLayout));
    require(retainedSource==newerData && retainedLayout==staleExpected,"Failure cleanup overwrote a newer owner");
    commonRoot.UpdateLayout();
    require(newerData.Size()==1 && newerData.GetAt(0)==newerButton,"Failure cleanup mutated the new source");
    // A later layout failure must restore through the unchanged owner and stop
    // applying, rather than enter an unbounded LayoutUpdated recovery loop.
    winrt::check_hresult(reinterpret_cast<PutSource>(table[7])(contract.get(),winrt::get_abi(data)));
    explorer::component_layout::Session failureSession;
    winrt::com_ptr<explorer::layout_contract::Attached> failureBaseline;
    explorer::repeater::Value failureExpected{nullptr};
    require(explorer::component_layout::ComponentLayoutFixture::bind(failureSession,commonRoot,view,tray,priorLayout,customFactory,failureBaseline,failureExpected,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{newApp,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},
         {input,taskbar::Component::InputIndicators},{quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),"Failure fixture did not bind");
    require(SUCCEEDED(failureSession.configure(nullptr,coordinatedRequest)),"Failure fixture initial placement refused");
    const auto confirmedPasses=failureSession.report(S_OK).passes;
    commonRoot.Width(180);commonRoot.Measure({180,48});commonRoot.Arrange({0,0,180,48});commonRoot.UpdateLayout();
    require(FAILED(failureSession.report(S_OK).status) && failureSession.report(S_OK).failurePhase!=0,"Narrower owner did not reject collisions");
    for(unsigned i=0;i<3;++i)commonRoot.UpdateLayout();
    require(failureSession.report(S_OK).passes==confirmedPasses,"Rejected layout retried on subsequent events");
    require(overflow.TransformToVisual(tray).TransformPoint({0,0}).X==0 && clock.TransformToVisual(tray).TransformPoint({0,0}).X==136,
        "Failed event did not return original tray controls to their owner slots");
    winrt::check_hresult(explorer::repeater::layout(instance,retainedLayout));
    require(retainedLayout==failureExpected && data.Size()==5,"Failed event replaced the original layout or model");
    commonRoot.Width(1000);commonRoot.Measure({1000,48});commonRoot.Arrange({0,0,1000,48});
    require(failureSession.reset(),"Failure session could not release observers");
    // Simulate a second layout owner fighting the requested positions. This
    // must stop within three attempts even with repeated LayoutUpdated events.
    explorer::component_layout::Session competing;
    winrt::com_ptr<explorer::layout_contract::Attached> competingBaseline;
    explorer::repeater::Value competingExpected{nullptr};
    bool forceOwner=false;
    const auto forceToken=commonRoot.LayoutUpdated([&](auto&&,auto&&) {
        if(!forceOwner)return;
        float x=0;
        for(auto button:{overflow,input,quick,clock}) {
            button.Arrange({x,0,static_cast<float>(button.Width()),48});x+=static_cast<float>(button.Width());
        }
    });
    require(explorer::component_layout::ComponentLayoutFixture::bind(competing,commonRoot,view,tray,priorLayout,customFactory,competingBaseline,competingExpected,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{newApp,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},
         {input,taskbar::Component::InputIndicators},{quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),"Competing-owner fixture did not bind");
    require(SUCCEEDED(competing.configure(nullptr,coordinatedRequest)),"Competing fixture initial placement refused");
    forceOwner=true;
    for(unsigned i=0;i<5;++i){tray.InvalidateArrange();commonRoot.UpdateLayout();}
    const auto competingReport=competing.report(S_OK);
    require(FAILED(competingReport.status) && competingReport.failurePhase==6 && competingReport.passes<=3,"Fighting owner did not stop within its layout budget");
    forceOwner=false;commonRoot.LayoutUpdated(forceToken);
    require(competing.reset(),"Competing-owner observer cleanup failed");
    // WinUI snapshots enumerable-only sources instead of observing their events.
    auto enumerableOnly=winrt::make_self<tests::EnumerableNotifySource>();
    for(const auto& item:data)enumerableOnly->rows.Append(item);
    const auto enumerableValue=enumerableOnly.as<explorer::repeater::Value>();
    const auto enumerableCapabilities=explorer::repeater::sourceCapabilities(enumerableValue);
    require(!enumerableCapabilities.liveIndexed() && (enumerableCapabilities.contracts&explorer::SourceNotifyCollection),"Enumerable notifications incorrectly granted indexed capability");
    winrt::check_hresult(reinterpret_cast<PutSource>(table[7])(contract.get(),winrt::get_abi(enumerableValue)));
    explorer::component_layout::Session copiedSourceSession;
    winrt::com_ptr<explorer::layout_contract::Attached> copiedBaseline;
    explorer::repeater::Value copiedExpected{nullptr};
    require(!explorer::component_layout::ComponentLayoutFixture::bind(copiedSourceSession,commonRoot,view,tray,priorLayout,customFactory,copiedBaseline,copiedExpected,{}),"Enumerable-only notifications granted live-model capability");
    require(enumerableOnly->subscriptions.empty(),"Rejected enumerable-only source retained a subscription");
    // The observed Explorer source advertises INotifyCollectionChanged rather
    // than IObservableVector. Exercise that branch with our own enumerable data.
    auto notifySource=winrt::make_self<tests::NotifyCollectionSource>();
    for(const auto& item:data)notifySource->rows.Append(item);
    const auto notifyValue=notifySource.as<explorer::repeater::Value>();
    const auto notifyCapabilities=explorer::repeater::sourceCapabilities(notifyValue);
    require(notifyCapabilities.liveIndexed() && (notifyCapabilities.contracts&explorer::SourceVectorView) && (notifyCapabilities.contracts&explorer::SourceNotifyCollection),"Read-only indexed notification contracts not detected");
    require(!notifyValue.try_as<winrt::Windows::Foundation::Collections::IVector<explorer::repeater::Value>>(),"Read-only fixture unexpectedly exposed vector mutation");
    require(!notifyValue.try_as<winrt::Windows::Foundation::Collections::IObservableVector<explorer::repeater::Value>>(),"Notify fixture unexpectedly exposed a writable observable model");
    winrt::check_hresult(reinterpret_cast<PutSource>(table[7])(contract.get(),winrt::get_abi(notifyValue)));
    commonRoot.UpdateLayout();
    const auto subscribersBefore=notifySource->subscriptions.size();
    explorer::component_layout::Session notifySession;
    winrt::com_ptr<explorer::layout_contract::Attached> notifyBaseline;
    explorer::repeater::Value notifyExpected{nullptr};
    require(explorer::component_layout::ComponentLayoutFixture::bind(notifySession,commonRoot,view,tray,priorLayout,customFactory,notifyBaseline,notifyExpected,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{newApp,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},
         {input,taskbar::Component::InputIndicators},{quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),"Notify-only source did not bind");
    require(notifySource->subscriptions.size()==subscribersBefore+1,"Notify branch did not subscribe exactly once");
    auto notifyRequest=coordinatedRequest;
    notifyRequest.entries[3].zone=1; // Leave room for the third app beside Search.
    require(SUCCEEDED(notifySession.configure(nullptr,notifyRequest)),"Notify-only source placement refused");
    Controls::Button notifiedApp;notifiedApp.Width(80);notifiedApp.Height(48);
    explorer::component_layout::ComponentLayoutFixture::addRole(notifySession,notifiedApp,taskbar::Component::Applications);
    const auto notifyGeneration=explorer::component_layout::ComponentLayoutFixture::generation(notifySession);
    notifySource->append(notifiedApp);
    require(explorer::component_layout::ComponentLayoutFixture::generation(notifySession)>notifyGeneration,"Notify event did not invalidate the cache");
    commonRoot.UpdateLayout();
    if(FAILED(notifySession.report(S_OK).status) || notifySession.report(S_OK).items!=10 || Media::VisualTreeHelper::GetParent(notifiedApp)!=view)
        std::cerr<<"Notify diagnostic status="<<std::hex<<notifySession.report(S_OK).status<<std::dec<<" items="<<notifySession.report(S_OK).items<<" phase="<<notifySession.report(S_OK).failurePhase<<" item="<<notifySession.report(S_OK).failureItem<<" parent="<<(Media::VisualTreeHelper::GetParent(notifiedApp)==view)<<'\n';
    require(SUCCEEDED(notifySession.report(S_OK).status) && notifySession.report(S_OK).items==10 && Media::VisualTreeHelper::GetParent(notifiedApp)==view,"Notify append did not place the original new application");
    notifySource->removeLast();commonRoot.UpdateLayout();
    require(SUCCEEDED(notifySession.report(S_OK).status) && notifySession.report(S_OK).items==9,"Notify removal retained a stale application binding");
    const auto idlePasses=notifySession.report(S_OK).passes;
    for(unsigned i=0;i<10;++i)commonRoot.UpdateLayout();
    require(notifySession.report(S_OK).passes==idlePasses,"Idle notify source repeatedly applied its layout");
    const auto pumpOwnedMessages=[] {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    };
    pumpOwnedMessages();
    for(unsigned i=0;i<4;++i) {
        notifySource->notify();commonRoot.UpdateLayout();
        require(SUCCEEDED(notifySession.report(S_OK).status),"Legitimate later source updates exhausted an old layout budget");
        pumpOwnedMessages();
    }
    require(notifySession.report(S_OK).passes>3,"Later dispatcher turns did not resume normal model updates");
    require(notifySession.reset(),"Notify source failed to reset");
    require(notifySource->subscriptions.size()==subscribersBefore,"Notify source retained the coordinator after reset");
    winrt::check_hresult(explorer::repeater::layout(instance,retainedLayout));
    require(retainedLayout==notifyExpected,"Notify reset replaced the original owner layout");
    // Repeated reset events must not replenish the recovery budget inside one
    // dispatcher turn: otherwise a layout/source feedback loop evades the cap.
    explorer::component_layout::Session notificationLoop;
    winrt::com_ptr<explorer::layout_contract::Attached> loopBaseline;
    explorer::repeater::Value loopExpected{nullptr};
    require(explorer::component_layout::ComponentLayoutFixture::bind(notificationLoop,commonRoot,view,tray,priorLayout,customFactory,loopBaseline,loopExpected,
        {{attachedFirst,taskbar::Component::Start},{attachedSecond,taskbar::Component::Search},{attachedThird,taskbar::Component::TaskView},
         {attachedNew,taskbar::Component::Applications},{newApp,taskbar::Component::Applications},{overflow,taskbar::Component::TrayOverflow},
         {input,taskbar::Component::InputIndicators},{quick,taskbar::Component::QuickSettings},{clock,taskbar::Component::Clock}}),"Notify-loop fixture did not bind");
    require(SUCCEEDED(notificationLoop.configure(nullptr,notifyRequest)),"Notify-loop initial placement refused");
    for(unsigned i=0;i<5;++i){notifySource->notify();tray.InvalidateArrange();commonRoot.UpdateLayout();}
    const auto loopReport=notificationLoop.report(S_OK);
    require(FAILED(loopReport.status) && loopReport.failurePhase==6 && loopReport.passes<=3,"Source reset events bypassed the layout attempt budget");
    require(notificationLoop.reset(),"Notify-loop observers did not release");
    require(notifySource->subscriptions.size()==subscribersBefore,"Notify-loop cleanup retained a subscription");
    std::cout<<"Passed: read-only indexed notify source, original app arrival/removal, idle coalescing, subscription cleanup and enumerable-only rejection; no Explorer.\n";
    std::cout<<"Passed: coordinated original central/tray controls across all three regions, original XAML hit-testing/parents, unchanged owner Layout, dynamic app arrival and reset in an owned HWND.\n";
    source.Content(nullptr); source.Close();
    std::cout << "Passed: actual WinUI 2 model reorder/restore, original UIElement identities and native model indices in an owned source.\n";
    std::cout << "Passed: actual system WinUI 2 ItemsRepeater public layout/index contract; no Explorer attachment.\n";
    // XAML metadata/factory caches can outlive WindowsXamlManager::Close.
    // Keep this one OS framework module loaded until this fixture process exits.
}
void exercise() {
    struct Window {
        HWND handle = CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"",WS_POPUP,
            -32000,-32000,1200,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ~Window() { if (handle) DestroyWindow(handle); }
    } window;
    require(window.handle!=nullptr,"private fixture HWND failed");
    Hosting::DesktopWindowXamlSource source;
    auto native = source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(native->AttachToWindow(window.handle));
    HWND child{}; winrt::check_hresult(native->get_WindowHandle(&child));
    require(SetWindowPos(child,nullptr,0,0,1000,48,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"private island sizing failed");
    Controls::StackPanel parent;
    parent.Orientation(Controls::Orientation::Horizontal);
    Controls::Button first, second, third;
    auto clicks = std::make_shared<std::array<unsigned,3>>();
    const std::array<Controls::Button,3> buttons{first,second,third};
    for (unsigned i=0; i<buttons.size(); ++i) {
        const auto& button = buttons[i];
        button.Width(44); button.Height(48);
        button.TabIndex(i);
        button.Click([clicks,i](auto&&,auto&&){ ++(*clicks)[i]; });
        button.HorizontalAlignment(HorizontalAlignment::Left);
        parent.Children().Append(button);
    }
    source.Content(parent);
    ShowWindow(child,SW_SHOWNOACTIVATE);
    ShowWindow(window.handle,SW_SHOWNOACTIVATE);
    parent.Measure({1000,48}); parent.Arrange({0,0,1000,48});
    parent.UpdateLayout();
    const auto started = GetTickCount64();
    while (!Media::VisualTreeHelper::GetParent(first) && GetTickCount64()-started<1000) {
        MSG message{};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        parent.UpdateLayout();
        MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    std::array<explorer::reorder::SlotBinding,3> bindings;
    const std::array roles{taskbar::Component::Start,taskbar::Component::Search,taskbar::Component::TaskView};
    std::array<taskbar::MeasuredItem,3> measured;
    for (unsigned i=0; i<3; ++i) {
        const auto& button = buttons[i];
        const auto box = button.TransformToVisual(parent).TransformBounds({0,0,44,48});
        bindings[i] = {i+1,button,{box.X,box.Y,box.Width,box.Height},Controls::Primitives::LayoutInformation::GetLayoutSlot(button)};
        measured[i] = {i+1,roles[i],bindings[i].measured,true};
    }
    explorer::reorder::LayoutSlotAdapter adapter(parent,bindings);
    auto duplicateBindings = bindings; duplicateBindings[1].element = duplicateBindings[0].element;
    explorer::reorder::LayoutSlotAdapter invalidAdapter(parent,duplicateBindings);
    taskbar::Rectangle invalidRead;
    require(!invalidAdapter.read(1,invalidRead),"same native object bound twice");
    for (unsigned i=0; i<3; ++i) {
        taskbar::Rectangle actual;
        if (!adapter.read(i+1,actual)) {
            std::cerr << "binding " << i << " parent=" << (Media::VisualTreeHelper::GetParent(buttons[i])==parent)
                << " thread=" << parent.Dispatcher().HasThreadAccess() << " root=" << (buttons[i].XamlRoot()==parent.XamlRoot())
                << " size=" << actual.width << ',' << actual.height << '\n';
            const auto actualParent = Media::VisualTreeHelper::GetParent(buttons[i]);
            std::wcerr << L"actualParent=" << (actualParent ? winrt::get_class_name(actualParent).c_str() : L"null")
                << L" root=" << static_cast<bool>(parent.XamlRoot()) << L" childSize=" << buttons[i].ActualWidth() << ',' << buttons[i].ActualHeight() << '\n';
            require(false,"native fixture binding invalid");
        }
    }
    const std::array request{taskbar::Assignment{roles[2],taskbar::Zone::Left},
        taskbar::Assignment{roles[0],taskbar::Zone::Center},taskbar::Assignment{roles[1],taskbar::Zone::Right}};
    const auto layout = taskbar::plan(measured,request,parent.ActualWidth(),48,8);
    require(layout.status == taskbar::PlanStatus::Ready,"XAML layout plan refused");
    const auto transaction = taskbar::transact(layout.moves,adapter);
    if (transaction!=taskbar::Transaction::Applied) std::cerr << "native transaction result=" << static_cast<unsigned>(transaction) << '\n';
    require(transaction == taskbar::Transaction::Applied,"real XAML slot writes failed verification");
    for (unsigned i=0; i<3; ++i) {
        require(parent.Children().GetAt(i) == buttons[i],"native parent collection order/identity changed");
        require(buttons[i].TabIndex()==static_cast<int>(i),"native tab index changed");
        const auto slot = Controls::Primitives::LayoutInformation::GetLayoutSlot(buttons[i]);
        require(buttons[i].Translation() == winrt::Windows::Foundation::Numerics::float3{},"layout strategy used Translation");
        taskbar::Rectangle actual; require(adapter.read(i+1,actual),"native object detached after arrange");
        const auto move = std::find_if(layout.moves.begin(),layout.moves.end(),[&](const auto& value){ return value.instance==i+1; });
        require(move!=layout.moves.end() && taskbar::same(actual,move->after) && std::abs(slot.X-move->after.x)<0.5,"layout slot differs from actual placement");
    }
    const HWND foreground = GetForegroundWindow();
    auto peer = Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(first);
    require(peer!=nullptr,"original button automation peer missing");
    peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    const auto invoked = GetTickCount64();
    while (!(*clicks)[0] && GetTickCount64()-invoked<1000) {
        MSG message{};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    require((*clicks)[0]==1 && (*clicks)[1]==0 && (*clicks)[2]==0,"original Click handler lost or wrong button invoked");
    require(GetForegroundWindow()==foreground,"private automation invocation changed global foreground");
    std::vector<taskbar::Move> undo;
    for (const auto& move : layout.moves) undo.push_back({move.instance,move.after,move.before});
    require(taskbar::transact(undo,adapter) == taskbar::Transaction::Applied,"original native slots not restored");
    // A parent-owned new layout pass can supersede an arranged slot. A snapshot
    // from before that pass must be rejected, not silently committed as current.
    require(taskbar::transact(layout.moves,adapter) == taskbar::Transaction::Applied,"second native apply failed");
    parent.InvalidateMeasure(); parent.Measure({1200,48});
    for (const auto& button : buttons) button.InvalidateArrange();
    parent.Arrange({0,0,1200,48});
    parent.UpdateLayout();
    taskbar::Rectangle current;
    require(adapter.read(1,current),"native parent reflow detached object");
    if (!taskbar::same(current,bindings[0].measured))
        std::cerr << "reflow actual=" << current.x << ',' << current.y << ',' << current.width << ',' << current.height
            << " original=" << bindings[0].measured.x << ',' << bindings[0].measured.y << ',' << bindings[0].measured.width << ',' << bindings[0].measured.height << '\n';
    require(taskbar::same(current,bindings[0].measured),"parent reflow did not restore its own layout slots");
    require(taskbar::transact(undo,adapter) == taskbar::Transaction::Stale,"parent reflow accepted stale snapshot");
    explorer::panel_order::Session collection;
    require(collection.bind(parent),"Static panel order could not bind");
    const std::vector<UIElement> logicalOrder{third,first,second};
    require(collection.apply(logicalOrder)==taskbar::Transaction::Applied,"Static native child collection did not reorder");
    require(parent.Children().GetAt(0)==third && first.TransformToVisual(parent).TransformPoint({0,0}).X>=43,"Collection order did not update real native slots");
    require(first.TabIndex()==0 && second.TabIndex()==1 && third.TabIndex()==2 && Media::VisualTreeHelper::GetParent(first)==parent,"Collection reorder changed original properties/parent");
    auto originalPeer=Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(first);
    originalPeer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
    MSG orderMessage{}; while(PeekMessageW(&orderMessage,nullptr,0,0,PM_REMOVE)){TranslateMessage(&orderMessage);DispatchMessageW(&orderMessage);}
    require((*clicks)[0]==2 && GetForegroundWindow()==foreground,"Static reordered button lost original Click or changed foreground");
    require(collection.reset() && parent.Children().GetAt(0)==first && parent.Children().GetAt(2)==third,"Static native child collection failed to restore");
    require(collection.bind(parent),"Static panel could not rebind after restore");
    Controls::Button dynamicPeer; parent.Children().Append(dynamicPeer); parent.UpdateLayout();
    require(collection.apply(logicalOrder)==taskbar::Transaction::Stale,"New system child was overwritten by stale order");
    require(!collection.reset() && parent.Children().Size()==4,"Reset overwrote a newer system collection");
    parent.Children().RemoveAt(3); parent.UpdateLayout();
    require(collection.reset(),"Fixture-owned cleanup could not release stale binding");
    parent.Children().Clear();
    Controls::Button fourth;fourth.Width(100);fourth.Height(48);
    Controls::Border conditional;conditional.Width(32);conditional.Height(48);
    const std::array<UIElement,4> groups{first,second,third,fourth};
    parent.Children().Append(first);parent.Children().Append(second);
    parent.Children().Append(conditional);parent.Children().Append(third);parent.Children().Append(fourth);
    parent.UpdateLayout();
    require(collection.bind(parent),"Four-group native fixture could not bind");
    for(unsigned code=0;code<24;++code) {
        conditional.Visibility(code%2 ? Visibility::Collapsed : Visibility::Visible);
        const auto order=*taskbar::trayOrder(code);
        const std::vector<UIElement> desired{groups[order[0]],groups[order[1]],conditional,groups[order[2]],groups[order[3]]};
        require(collection.apply(desired)==taskbar::Transaction::Applied,"One of 24 native group permutations failed");
        require(parent.Children().GetAt(2)==conditional && fourth.Width()==100,"Unknown fixed slot or variable group width changed");
    }
    require(collection.reset() && parent.Children().GetAt(0)==first && parent.Children().GetAt(4)==fourth,"Four-group native reset failed");
    source.Content(nullptr); source.Close();
    std::cout << "Passed: original Windows XAML buttons in private offscreen HWND, layout slots, same parent/Translation/TabIndex, original Click via automation, restore and stale reflow rejection. Explorer/foreground untouched.\n";
}
int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager = Hosting::WindowsXamlManager::InitializeForCurrentThread();
        exerciseLayoutRequest();
        exercise();
        exerciseRepeaterContract();
        manager.Close();
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << "XAML fixture failed: HRESULT " << std::hex << static_cast<unsigned>(error.code().value) << '\n';
        std::wcerr<<error.message().c_str()<<'\n';return 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
