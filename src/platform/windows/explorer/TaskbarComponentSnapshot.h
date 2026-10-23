#pragma once
#include "TaskbarContextMenu.h"
#include "TaskbarModuleProtocol.h"
#include "TaskbarRepeaterAccess.h"
#include "TaskbarSourceCapabilities.h"
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
inline std::unique_ptr<ComponentSnapshot> inspect(HWND hwnd) noexcept {
    // Never reserve the bounded diagnostic packet on Explorer's message stack.
    auto snapshot=std::unique_ptr<ComponentSnapshot>(new(std::nothrow) ComponentSnapshot);
    if(!snapshot)return {};
    auto& result=*snapshot;
    try {
        auto root = context_menu::taskbarElement(hwnd);
        if (!root || !root.XamlRoot()) return snapshot;
        // Include SystemTray siblings of TaskbarFrame without crossing XamlRoot.
        for (unsigned i = 0; i < 8; ++i) {
            auto parent = Media::VisualTreeHelper::GetParent(root).try_as<FrameworkElement>();
            if (!parent) break;
            if (parent.XamlRoot() != root.XamlRoot() || i == 7) return snapshot;
            root = parent;
        }
        std::vector<DependencyObject> pending{root};
        struct ParentLayout { FrameworkElement parent; HRESULT status; winrt::hstring type, sourceType; repeater::SourceCapabilities source; };
        std::vector<ParentLayout> layouts;
        std::vector<FrameworkElement> owners;
        unsigned visited = 0;
        while (!pending.empty()) {
            if (++visited > 512) { result.status = E_ABORT; return snapshot; }
            auto node = pending.back(); pending.pop_back();
            if (auto element = node.try_as<FrameworkElement>()) {
                const auto type = winrt::get_class_name(element);
                const auto role = taskbar::identify(type.c_str(), element.Name().c_str(),
                    Automation::AutomationProperties::GetAutomationId(element).c_str());
                auto parent = Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
                // Include original tray layout slots, including collapsed and
                // conditional groups, without assigning movement capability.
                const bool traySlot=parent && parent.Name()==L"SystemTrayFrameGrid" && parent.try_as<Controls::StackPanel>();
                if (role != taskbar::Component::Unknown || traySlot) {
                    if (result.count == maxComponentSnapshot) { result.status = E_ABORT; return snapshot; }
                    auto& entry = result.entries[result.count++];
                    wcsncpy_s(entry.id, traySlot && role==taskbar::Component::Unknown ? L"tray-slot" : taskbar::stableId(role), _TRUNCATE);
                    wcsncpy_s(entry.type, type.c_str(), _TRUNCATE);
                    wcsncpy_s(entry.name,element.Name().c_str(),_TRUNCATE);
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
                            repeater::Value layout{nullptr},source{nullptr};
                            const HRESULT status=repeater::layout(owner.container,layout);
                            const auto sourceStatus=repeater::itemsSource(owner.container,source);
                            const auto capabilities=SUCCEEDED(sourceStatus) ? repeater::sourceCapabilities(source) : repeater::SourceCapabilities{sourceStatus,0};
                            layouts.push_back({owner.container,status,SUCCEEDED(status)&&layout ? winrt::get_class_name(layout) : winrt::hstring{},
                                SUCCEEDED(sourceStatus)&&source ? winrt::get_class_name(source) : winrt::hstring{},capabilities});
                            cached=std::prev(layouts.end());
                        }
                        entry.containerId=static_cast<unsigned>(cached-layouts.begin())+1;
                        entry.layoutStatus=cached->status;
                        entry.sourceStatus=cached->source.status;
                        entry.sourceContracts=cached->source.contracts;
                        wcsncpy_s(entry.layoutType,cached->type.c_str(),_TRUNCATE);
                        wcsncpy_s(entry.sourceType,cached->sourceType.c_str(),_TRUNCATE);
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
            if (count < 0 || pending.size() + count > 512) { result.status = E_ABORT; return snapshot; }
            for (int i = 0; i < count; ++i) pending.push_back(Media::VisualTreeHelper::GetChild(node, i));
        }
        result.visited = visited; result.status = S_OK;
    } catch (...) { result.status = winrt::to_hresult(); }
    return snapshot;
}
}
