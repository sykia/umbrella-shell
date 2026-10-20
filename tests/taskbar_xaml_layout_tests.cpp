// Private-process offscreen tool-window XAML island: no Explorer or global input.
#include <Windows.h>
#undef GetCurrentTime
#include "platform/windows/explorer/TaskbarLayoutSlotAdapter.h"
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
    source.Content(nullptr); source.Close();
    std::cout << "Passed: original Windows XAML buttons in private offscreen HWND, layout slots, same parent/Translation/TabIndex, original Click via automation, restore and stale reflow rejection. Explorer/foreground untouched.\n";
}
int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager = Hosting::WindowsXamlManager::InitializeForCurrentThread();
        exercise();
        manager.Close();
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::cerr << "XAML fixture failed: HRESULT " << std::hex << static_cast<unsigned>(error.code().value) << '\n'; return 1;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
