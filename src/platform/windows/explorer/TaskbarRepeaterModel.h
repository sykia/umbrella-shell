#pragma once
#include "TaskbarRepeaterAccess.h"
#include "core/taskbar/ModelOrder.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>

namespace umbrella::explorer::repeater {
// Only for a source the caller owns. Never obtain write authority by casting
// Explorer's ItemsSource to IObservableVector. Foreign models remain unsupported.
// No source replacement, reparenting, polling or retained global XAML objects.
template<class Verify>
taskbar::Transaction reorderOwnedModel(const Value& repeater,
    const winrt::Windows::Foundation::Collections::IObservableVector<Value>& source,
    const std::vector<Value>& before, const std::vector<Value>& after, Verify verify) noexcept {
    struct Adapter {
        const Value& repeater;
        const winrt::Windows::Foundation::Collections::IObservableVector<Value>& source;
        Verify& verifier;
        bool attached() const {
            auto element=repeater.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
            Value current{nullptr};
            return element && element.Dispatcher().HasThreadAccess() && source &&
                SUCCEEDED(itemsSource(repeater,current)) && current==source;
        }
        bool snapshot(std::vector<Value>& result) const {
            if (!attached()) return false;
            const auto count=source.Size();
            if (count>taskbar::maxReorderItems) return false;
            result.clear(); result.reserve(count);
            for(unsigned i=0;i<count;++i) {
                auto item=source.GetAt(i);
                if (!item) return false;
                result.push_back(std::move(item));
            }
            return attached() && source.Size()==count;
        }
        bool remove(unsigned index) const {
            if (!attached()) return false;
            source.RemoveAt(index); return true;
        }
        bool insert(unsigned index,const Value& value) const {
            if (!attached()) return false;
            source.InsertAt(index,value); return true;
        }
        bool verify(const std::vector<Value>& order) const { return attached() && verifier(order); }
    } adapter{repeater,source,verify};
    return taskbar::reorderModel(before,after,adapter);
}
}
