#pragma once
#include <inspectable.h>
#undef GetCurrentTime
#include <winrt/Windows.UI.Xaml.h>

namespace umbrella::explorer::repeater {
using Value = winrt::Windows::Foundation::IInspectable;
// WinUI 2 IItemsRepeater's public, versioned WinRT interface. This narrow
// accessor needs no WinUI 3 projection, private RVA, code patch or reparenting.
// Slots are IInspectable(6), ItemsSource(2), ItemsSourceView(1), ItemTemplate(2), Layout.
inline constexpr GUID itemsRepeaterId{0x0bd894f2,0xedfc,0x5ddf,{0xa1,0x66,0x2d,0xb1,0x4b,0xbf,0xdf,0x35}};
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
}
