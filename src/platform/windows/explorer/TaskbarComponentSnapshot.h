#pragma once
#include "TaskbarContextMenu.h"
#include "TaskbarModuleProtocol.h"
#include "TaskbarRepeaterAccess.h"
#include "core/taskbar/ComponentRegistry.h"
#include "core/taskbar/Placement.h"
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <vector>
#include <algorithm>
#include <iterator>

namespace umbrella::explorer::components {
using namespace winrt::Windows::UI::Xaml;
// Called only by an explicit diagnostic request on the existing Explorer UI
// thread. Bounded snapshot, no retained XAML objects, subscriptions or polling.
inline ComponentSnapshot inspect(HWND hwnd) noexcept {
    ComponentSnapshot result;
    try {
        auto root = context_menu::taskbarElement(hwnd);
        if (!root || !root.XamlRoot()) return result;
        // Include SystemTray siblings of TaskbarFrame without crossing XamlRoot.
        for (unsigned i = 0; i < 8; ++i) {
            auto parent = Media::VisualTreeHelper::GetParent(root).try_as<FrameworkElement>();
            if (!parent) break;
            if (parent.XamlRoot() != root.XamlRoot() || i == 7) return result;
            root = parent;
        }
        std::vector<DependencyObject> pending{root};
        struct ParentLayout { FrameworkElement parent; HRESULT status; winrt::hstring type; };
        std::vector<ParentLayout> layouts;
        std::vector<FrameworkElement> owners;
        unsigned visited = 0;
        while (!pending.empty()) {
            if (++visited > 512) { result.status = E_ABORT; return result; }
            auto node = pending.back(); pending.pop_back();
            if (auto element = node.try_as<FrameworkElement>()) {
                const auto type = winrt::get_class_name(element);
                const auto role = taskbar::identify(type.c_str(), element.Name().c_str(),
                    Automation::AutomationProperties::GetAutomationId(element).c_str());
                if (role != taskbar::Component::Unknown) {
                    if (result.count == maxComponentSnapshot) { result.status = E_ABORT; return result; }
                    auto& entry = result.entries[result.count++];
                    wcsncpy_s(entry.id, taskbar::stableId(role), _TRUNCATE);
                    wcsncpy_s(entry.type, type.c_str(), _TRUNCATE);
                    auto parent = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
                    if (parent) {
                        wcsncpy_s(entry.parentType, winrt::get_class_name(parent).c_str(), _TRUNCATE);
                        wcsncpy_s(entry.parentName, parent.Name().c_str(), _TRUNCATE);
                    }
                    repeater::Owner owner;
                    entry.ownerStatus=repeater::owner(element,owner);
                    if (SUCCEEDED(entry.ownerStatus)) {
                        entry.ownerDepth=owner.depth;
                        entry.modelIndex=owner.index;
                        wcsncpy_s(entry.ownerType,winrt::get_class_name(owner.element).c_str(),_TRUNCATE);
                        auto identity=std::find(owners.begin(),owners.end(),owner.element);
                        if (identity==owners.end()) { owners.push_back(owner.element); identity=std::prev(owners.end()); }
                        entry.ownerId=static_cast<unsigned>(identity-owners.begin())+1;
                        auto cached=std::find_if(layouts.begin(),layouts.end(),[&](const auto& value){return value.parent==owner.container;});
                        if (cached==layouts.end()) {
                            repeater::Value layout{nullptr};
                            const HRESULT status=repeater::layout(owner.container,layout);
                            layouts.push_back({owner.container,status,SUCCEEDED(status)&&layout ? winrt::get_class_name(layout) : winrt::hstring{}});
                            cached=std::prev(layouts.end());
                        }
                        entry.containerId=static_cast<unsigned>(cached-layouts.begin())+1;
                        entry.layoutStatus=cached->status;
                        wcsncpy_s(entry.layoutType,cached->type.c_str(),_TRUNCATE);
                    }
                    const auto box = element.TransformToVisual(root).TransformBounds(
                        {0, 0, static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
                    entry.x = box.X; entry.y = box.Y; entry.width = box.Width; entry.height = box.Height;
                    const taskbar::Rectangle measured{box.X,box.Y,box.Width,box.Height};
                    const taskbar::Rectangle viewport{0,0,root.ActualWidth(),root.ActualHeight()};
                    // Recycled task-list buttons can retain Visibility::Visible
                    // at negative offscreen coordinates. They are not live peers.
                    entry.visible = element.Visibility() == Visibility::Visible &&
                        taskbar::valid(measured) && taskbar::valid(viewport) && taskbar::intersects(measured,viewport);
                    entry.hitTestVisible = element.IsHitTestVisible();
                    // Discovery alone never grants a movement capability.
                }
            }
            const int count = Media::VisualTreeHelper::GetChildrenCount(node);
            if (count < 0 || pending.size() + count > 512) { result.status = E_ABORT; return result; }
            for (int i = 0; i < count; ++i) pending.push_back(Media::VisualTreeHelper::GetChild(node, i));
        }
        result.visited = visited; result.status = S_OK;
    } catch (...) { result.status = winrt::to_hresult(); }
    return result;
}
}
