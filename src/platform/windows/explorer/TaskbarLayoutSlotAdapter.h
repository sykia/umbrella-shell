#pragma once
#include "core/taskbar/Reorder.h"
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>

namespace umbrella::explorer::reorder {
using namespace winrt::Windows::UI::Xaml;
struct SlotBinding {
    std::uint64_t instance{};
    FrameworkElement element{nullptr};
    taskbar::Rectangle measured;
    winrt::Windows::Foundation::Rect slot{};
};
// Layout-slot strategy, distinct from Translation. The caller must own the
// layout pass. A matching visual parent alone does not establish such ownership.
// Exercised only by the private-process XAML fixture; NOT enabled for Explorer.
class LayoutSlotAdapter {
    FrameworkElement parent_{nullptr};
    bool bindingsValid_ = true;
    std::span<const SlotBinding> bindings_;
    const SlotBinding* binding(std::uint64_t instance) const {
        if (!bindingsValid_) return nullptr;
        for (const auto& item : bindings_) if (item.instance == instance) return &item;
        return nullptr;
    }
    bool attached(const SlotBinding& binding) const {
        return parent_ && binding.element && parent_.Dispatcher().HasThreadAccess() &&
            binding.element.XamlRoot() == parent_.XamlRoot() &&
            Media::VisualTreeHelper::GetParent(binding.element) == parent_ &&
            binding.element.Visibility() == Visibility::Visible;
    }
public:
    LayoutSlotAdapter(FrameworkElement parent, std::span<const SlotBinding> bindings)
        : parent_(std::move(parent)), bindings_(bindings) {
        if (!parent_ || bindings.empty() || bindings.size()>taskbar::maxReorderItems) { bindingsValid_=false; return; }
        for (size_t i=0; i<bindings.size(); ++i) {
            const auto& item=bindings[i];
            if (!item.instance || !item.element || !taskbar::valid(item.measured) ||
                !taskbar::valid({item.slot.X,item.slot.Y,item.slot.Width,item.slot.Height})) bindingsValid_=false;
            for (size_t j=0; j<i; ++j)
                if (item.instance==bindings[j].instance || item.element==bindings[j].element) bindingsValid_=false;
        }
    }
    bool read(std::uint64_t instance, taskbar::Rectangle& result) const {
        const auto* item = binding(instance);
        if (!item || !attached(*item)) return false;
        const auto box = item->element.TransformToVisual(parent_).TransformBounds(
            {0, 0, static_cast<float>(item->element.ActualWidth()), static_cast<float>(item->element.ActualHeight())});
        result = {box.X, box.Y, box.Width, box.Height};
        return taskbar::valid(result);
    }
    bool write(std::uint64_t instance, taskbar::Rectangle target) const {
        const auto* item = binding(instance);
        if (!item || !attached(*item) || !taskbar::inside(target,parent_.ActualWidth(),parent_.ActualHeight()) ||
            std::abs(target.width-item->measured.width)>0.5 || std::abs(target.height-item->measured.height)>0.5) return false;
        auto slot = item->slot;
        slot.X += static_cast<float>(target.x - item->measured.x);
        slot.Y += static_cast<float>(target.y - item->measured.y);
        // Arrange updates the real layout slot, preserving native margins and
        // the original element, input handlers and parent/model collection.
        item->element.Arrange(slot);
        return true;
    }
    bool accept(std::uint64_t instance) const {
        const auto* item = binding(instance);
        taskbar::Rectangle actual;
        if (!item || !read(instance,actual) || !item->element.IsHitTestVisible() ||
            !taskbar::inside(actual,parent_.ActualWidth(),parent_.ActualHeight())) return false;
        const auto slot = Controls::Primitives::LayoutInformation::GetLayoutSlot(item->element);
        return std::abs((slot.X-item->slot.X)-(actual.x-item->measured.x))<=0.5 &&
            std::abs((slot.Y-item->slot.Y)-(actual.y-item->measured.y))<=0.5 &&
            std::abs(slot.Width-item->slot.Width)<=0.5 && std::abs(slot.Height-item->slot.Height)<=0.5;
    }
};
}
