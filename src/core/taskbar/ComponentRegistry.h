#pragma once
#include <string_view>

namespace umbrella::taskbar {
enum class Component { Unknown, Widgets, Start, Search, TaskView, Applications, Clock, QuickSettings, Desktop, TrayIcons, TrayOverflow, InputIndicators };
// Match the outer functional object, never its glyph/text descendants. App task
// list instances deliberately share a role: an ordinal is not a persistent AppID.
inline Component identify(std::wstring_view type, std::wstring_view name, std::wstring_view automation) noexcept {
    if (type == L"Taskbar.AugmentedEntryPointButton") return Component::Widgets;
    if (type == L"Taskbar.TaskListButton") return Component::Applications;
    if (type == L"SearchUx.SearchUI.SearchButtonControl") return Component::Search;
    if (type == L"SystemTray.OmniButton") {
        if (name == L"NotificationCenterButton") return Component::Clock;
        if (name == L"ControlCenterButton") return Component::QuickSettings;
    }
    if (type == L"SystemTray.Stack" && name == L"ShowDesktopStack") return Component::Desktop;
    if (type == L"SystemTray.Stack" && name == L"NotifyIconStack") return Component::TrayOverflow;
    if (type == L"SystemTray.Stack" && name == L"NonActivatableStack") return Component::InputIndicators;
    if (type == L"SystemTray.NotificationAreaIcons") return Component::TrayIcons;
    // Automation identity is only meaningful on the original interactive object.
    if (type == L"Taskbar.ExperienceToggleButton" || type == L"Taskbar.SearchBoxButton" ||
        type == L"Windows.UI.Xaml.Controls.Button" || type == L"Windows.UI.Xaml.Controls.Primitives.ToggleButton" ||
        type == L"Windows.UI.Xaml.Controls.ToggleButton") {
        if (automation == L"StartButton" || name == L"StartButton") return Component::Start;
        if (automation == L"SearchButton" || name == L"SearchButton") return Component::Search;
        if (automation == L"TaskViewButton" || name == L"TaskViewButton") return Component::TaskView;
    }
    return Component::Unknown;
}
inline const wchar_t* stableId(Component component) noexcept {
    switch (component) {
    case Component::Widgets: return L"widgets";
    case Component::Start: return L"start";
    case Component::Search: return L"search";
    case Component::TaskView: return L"taskview";
    case Component::Applications: return L"apps";
    case Component::Clock: return L"clock";
    case Component::QuickSettings: return L"quicksettings";
    case Component::Desktop: return L"desktop";
    case Component::TrayIcons: return L"trayapps";
    case Component::TrayOverflow: return L"overflow";
    case Component::InputIndicators: return L"input";
    default: return L"unknown";
    }
}
}
