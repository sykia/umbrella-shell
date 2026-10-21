#pragma once
#include <inspectable.h>
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Core.h>

namespace umbrella::explorer::repeater {
using Value = winrt::Windows::Foundation::IInspectable;
// WinUI 2 IItemsRepeater's public, versioned WinRT interface. This narrow
// accessor needs no WinUI 3 projection, private RVA, code patch or reparenting.
// Slots are IInspectable(6), ItemsSource(2), ItemsSourceView(1), ItemTemplate(2), Layout.
inline constexpr GUID itemsRepeaterId{0x0bd894f2,0xedfc,0x5ddf,{0xa1,0x66,0x2d,0xb1,0x4b,0xbf,0xdf,0x35}};
inline HRESULT itemsSource(const Value& element, Value& result) noexcept {
    result=nullptr;
    if (!element) return E_INVALIDARG;
    try {
        winrt::com_ptr<::IInspectable> contract;
        const auto queried=reinterpret_cast<::IUnknown*>(winrt::get_abi(element))->QueryInterface(itemsRepeaterId,contract.put_void());
        if (FAILED(queried)) return queried;
        using Getter=HRESULT(STDMETHODCALLTYPE*)(void*,void**);
        auto** table=*reinterpret_cast<void***>(contract.get());
        return reinterpret_cast<Getter>(table[6])(contract.get(),winrt::put_abi(result));
    } catch (...) { return winrt::to_hresult(); }
}
inline HRESULT layout(const Value& element, Value& result) noexcept {
    result = nullptr;
    if (!element) return E_INVALIDARG;
    try {
        winrt::com_ptr<::IInspectable> contract;
        const auto queried = reinterpret_cast<::IUnknown*>(winrt::get_abi(element))->QueryInterface(
            itemsRepeaterId, contract.put_void());
        if (FAILED(queried)) return queried;
        using Getter = HRESULT(STDMETHODCALLTYPE*)(void*,void**);
        auto** table = *reinterpret_cast<void***>(contract.get());
        const auto getLayout = reinterpret_cast<Getter>(table[11]);
        return getLayout(contract.get(),winrt::put_abi(result));
    } catch (...) { return winrt::to_hresult(); }
}
inline HRESULT elementIndex(const Value& element, const Value& child, int& result) noexcept {
    result = -1;
    if (!element || !child) return E_INVALIDARG;
    try {
        winrt::com_ptr<::IInspectable> contract;
        const auto queried = reinterpret_cast<::IUnknown*>(winrt::get_abi(element))->QueryInterface(itemsRepeaterId,contract.put_void());
        if (FAILED(queried)) return queried;
        // IInspectable(6) + thirteen property accessors; GetElementIndex is 19.
        using Getter = HRESULT(STDMETHODCALLTYPE*)(void*,void*,int*);
        auto** table = *reinterpret_cast<void***>(contract.get());
        return reinterpret_cast<Getter>(table[19])(contract.get(),winrt::get_abi(child),&result);
    } catch (...) { return winrt::to_hresult(); }
}
struct Owner {
    winrt::Windows::UI::Xaml::FrameworkElement container{nullptr}, element{nullptr};
    int index=-1;
    unsigned depth=0;
};
// Resolve the nearest owning repeater item, not an arbitrary glyph descendant.
// This is bounded ancestry inspection, not a visual-tree rescan or write grant.
inline HRESULT owner(const winrt::Windows::UI::Xaml::FrameworkElement& child, Owner& result) noexcept {
    result={};
    if (!child) return E_INVALIDARG;
    try {
        using namespace winrt::Windows::UI::Xaml;
        if (!child.Dispatcher().HasThreadAccess()) return RPC_E_WRONG_THREAD;
        const auto root=child.XamlRoot();
        if (!root) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        auto item=child;
        for(unsigned depth=0;depth<8;++depth) {
            auto parent=Media::VisualTreeHelper::GetParent(item).try_as<FrameworkElement>();
            if (!parent) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            if (parent.XamlRoot()!=root) return E_ABORT;
            int index=-1;
            const auto status=elementIndex(parent,item,index);
            if (SUCCEEDED(status)) {
                if (index<0) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
                result={parent,item,index,depth};
                return S_OK;
            }
            if (status!=E_NOINTERFACE) return status;
            item=parent;
        }
        return E_BOUNDS;
    } catch (...) { return winrt::to_hresult(); }
}
}
