// Private-process offscreen tool-window XAML island: no Explorer or global input.
#include <Windows.h>
#undef GetCurrentTime
#include "platform/windows/explorer/TaskbarLayoutSlotAdapter.h"
#include "platform/windows/explorer/TaskbarRepeaterAccess.h"
#include "platform/windows/explorer/TaskbarRepeaterModel.h"
#include "platform/windows/explorer/TaskbarPanelOrder.h"
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
    source.Content(nullptr); source.Close();
    std::cout << "Passed: original Windows XAML buttons in private offscreen HWND, layout slots, same parent/Translation/TabIndex, original Click via automation, restore and stale reflow rejection. Explorer/foreground untouched.\n";
}
int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager = Hosting::WindowsXamlManager::InitializeForCurrentThread();
        exercise();
        exerciseRepeaterContract();
        manager.Close();
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << "XAML fixture failed: HRESULT " << std::hex << static_cast<unsigned>(error.code().value) << '\n'; return 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
