#pragma once
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <algorithm>
#include <vector>

namespace umbrella::tests {
// Owns its data, exposing notifications and a read-only index, no writable vector
// interface to the consumer. This exercises Explorer's observed source contract
// without accessing Explorer or pretending to implement its private model.
template<class D,class... IndexedInterfaces>
struct NotifyCollectionSourceBase : winrt::implements<D,
    winrt::Windows::UI::Xaml::Interop::INotifyCollectionChanged,
    winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Foundation::IInspectable>,IndexedInterfaces...> {
    using Value=winrt::Windows::Foundation::IInspectable;
    using Handler=winrt::Windows::UI::Xaml::Interop::NotifyCollectionChangedEventHandler;
    winrt::Windows::Foundation::Collections::IVector<Value> rows=winrt::single_threaded_vector<Value>();
    winrt::event<Handler> changed;
    std::vector<winrt::event_token> subscriptions;
    auto First(){return rows.First();}
    auto GetAt(uint32_t index){return rows.GetAt(index);}
    uint32_t Size(){return rows.Size();}
    bool IndexOf(const Value& value,uint32_t& index){return rows.IndexOf(value,index);}
    uint32_t GetMany(uint32_t start,winrt::array_view<Value> values){return rows.GetView().GetMany(start,values);}
    winrt::event_token CollectionChanged(const Handler& handler) {
        if(subscriptions.size()>=16)winrt::throw_hresult(E_BOUNDS);
        const auto token=changed.add(handler);subscriptions.push_back(token);return token;
    }
    void CollectionChanged(winrt::event_token token) noexcept {
        changed.remove(token);std::erase(subscriptions,token);
    }
    void notify() {
        using namespace winrt::Windows::UI::Xaml::Interop;
        changed(static_cast<Value>(*static_cast<D*>(this)),NotifyCollectionChangedEventArgs(NotifyCollectionChangedAction::Reset,nullptr,nullptr,-1,-1));
    }
    void append(const Value& value) {
        if(rows.Size()>=64)winrt::throw_hresult(E_BOUNDS);
        rows.Append(value);notify();
    }
    void removeLast(){rows.RemoveAtEnd();notify();}
};
struct NotifyCollectionSource : NotifyCollectionSourceBase<NotifyCollectionSource,
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>> {};
struct EnumerableNotifySource : NotifyCollectionSourceBase<EnumerableNotifySource> {};
}
