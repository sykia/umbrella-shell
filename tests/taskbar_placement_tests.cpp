#include "core/taskbar/Placement.h"
#include "core/taskbar/TrayOrder.h"
#include "core/taskbar/ComponentRegistry.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace umbrella::taskbar;
void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
    for(unsigned code=0;code<24;++code) {
        const auto order=trayOrder(code);
        require(order && trayOrderCode(*order)==code,"tray permutation does not round-trip");
        auto sorted=*order;std::sort(sorted.begin(),sorted.end());
        require(sorted==TrayOrder{0,1,2,3},"tray group duplicated or missing");
    }
    require(trayOrder(1)==TrayOrder{0,1,3,2},"saved Clock/Quick Settings order changed meaning");
    require(!trayOrder(24) && !trayOrderCode(TrayOrder{0,0,2,3}),"invalid tray permutation accepted");
    require(identify(L"Windows.UI.Xaml.Controls.ToggleButton", L"StartButton", L"") == Component::Start, "Start identity lost");
    require(identify(L"Taskbar.TaskListButton", L"TaskListButton", L"private-app-id") == Component::Applications, "task list role lost");
    require(identify(L"SystemTray.OmniButton", L"ControlCenterButton", L"") == Component::QuickSettings, "Quick Settings split into fake independent controls");
    require(identify(L"SystemTray.OmniButton", L"NotificationCenterButton", L"") == Component::Clock, "clock group lost");
    require(identify(L"SystemTray.Stack",L"NotifyIconStack",L"")==Component::TrayOverflow,"original icon stack lost");
    require(identify(L"SystemTray.Stack",L"NonActivatableStack",L"")==Component::InputIndicators,"original input group lost");
    require(identify(L"SystemTray.Stack",L"UnknownStack",L"")==Component::Unknown,"unknown system group granted identity");
    require(identify(L"Windows.UI.Xaml.Controls.TextBlock", L"StartButton", L"StartButton") == Component::Unknown, "glyph classified as functional button");
    require(identify(L"Unknown.NewControl", L"StartButton", L"StartButton") == Component::Unknown, "unknown container granted identity");
    const std::array obstacles{Rectangle{0, 0, 216, 48}, Rectangle{500, 0, 300, 48}};
    require(verified({224, 0, 44, 48}, 224, 1000, 48, obstacles), "free placement refused");
    require(!verified({210, 0, 44, 48}, 210, 1000, 48, obstacles), "Widgets collision accepted");
    require(!verified({470, 0, 44, 48}, 470, 1000, 48, obstacles), "task list collision accepted");
    require(!verified({300, 0, 44, 48}, 224, 1000, 48, obstacles), "ignored native write acknowledged");
    require(!verified({980, 0, 44, 48}, 980, 1000, 48, obstacles), "right overflow accepted");
    require(!verified({224, -1, 44, 48}, 224, 1000, 48, obstacles), "vertical clipping accepted");
    require(!verified({224, 0, 44, 0}, 224, 1000, 48, obstacles), "unloaded peer accepted");
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    require(!verified({nan, 0, 44, 48}, 224, 1000, 48, obstacles), "NaN coordinate accepted");
    require(!verified({224, 0, 44, 48}, nan, 1000, 48, obstacles), "NaN request accepted");
    require(!inside({224, 0, 44, 48}, nan, 48), "NaN parent accepted");
    require(!valid({0, 0, std::numeric_limits<double>::infinity(), 48}), "infinite extent accepted");
    require(!intersects({216, 0, 44, 48}, obstacles[0]), "touching edges treated as collision");
    require(!intersects({0, 49, 44, 48}, obstacles[0]), "separate rows treated as collision");
    require(intersects({0, 0, 44, 48}, {nan, 0, 20, 48}), "invalid obstacle accepted");
    // Simulate a dynamic task-list entry appearing in formerly free space.
    const std::array changed{obstacles[0], Rectangle{250, 0, 44, 48}};
    require(!verified({224, 0, 44, 48}, 224, 1000, 48, changed), "new application collision ignored");
    std::cout << "Passed: placement bounds, collision, stale write, dynamic obstacles and invalid geometry.\n";
}
