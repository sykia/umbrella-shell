// Experimental public-layout bridge for owned-process tests only.
#pragma once
#include "platform/windows/explorer/TaskbarRepeaterAccess.h"
#include <winrt/Windows.Foundation.h>
#include <functional>

// Narrow projection of the versioned public WinUI 2 layout contract. GUIDs and
// signatures match Microsoft.UI.Xaml.CBS metadata, not private Explorer RVAs.
namespace umbrella::explorer::layout_contract {
inline winrt::Windows::Foundation::IInspectable value(void* pointer) {
    winrt::Windows::Foundation::IInspectable result{nullptr};winrt::copy_from_abi(result,pointer);return result;
}
struct Overrides : winrt::Windows::Foundation::IInspectable {
    using IInspectable::IInspectable;
};
struct LayoutInterface : winrt::Windows::Foundation::IInspectable {using IInspectable::IInspectable;};
struct LayoutView : winrt::Windows::Foundation::IInspectable {using IInspectable::IInspectable;};
}
namespace winrt::impl {
template<> struct category<umbrella::explorer::layout_contract::Overrides> {using type=interface_category;};
template<> struct category<umbrella::explorer::layout_contract::LayoutInterface> {using type=interface_category;};
template<> struct category<umbrella::explorer::layout_contract::LayoutView> {using type=class_category;};
template<> struct default_interface<umbrella::explorer::layout_contract::LayoutView> {using type=umbrella::explorer::layout_contract::LayoutInterface;};
template<> inline constexpr guid guid_v<umbrella::explorer::layout_contract::LayoutInterface>{0x24e50c1c,0x9c51,0x5144,{0x9d,0xdc,0x3f,0x50,0x01,0x91,0xc2,0x62}};
template<> inline constexpr auto& name_v<umbrella::explorer::layout_contract::LayoutView> = L"Microsoft.UI.Xaml.Controls.Layout";
template<> inline constexpr guid guid_v<umbrella::explorer::layout_contract::Overrides>{0xd9996f1e,0x62be,0x515e,{0x84,0x3d,0x08,0x35,0x06,0xdf,0x3e,0x7d}};
template<> inline constexpr auto& name_v<umbrella::explorer::layout_contract::Overrides> = L"Microsoft.UI.Xaml.Controls.IVirtualizingLayoutOverrides";
template<> struct abi<umbrella::explorer::layout_contract::Overrides> {
    struct WINRT_IMPL_NOVTABLE type : inspectable_abi {
        virtual int32_t __stdcall InitializeForContextCore(void*) noexcept=0;
        virtual int32_t __stdcall UninitializeForContextCore(void*) noexcept=0;
        virtual int32_t __stdcall MeasureOverride(void*,Windows::Foundation::Size,Windows::Foundation::Size*) noexcept=0;
        virtual int32_t __stdcall ArrangeOverride(void*,Windows::Foundation::Size,Windows::Foundation::Size*) noexcept=0;
        virtual int32_t __stdcall OnItemsChangedCore(void*,void*,void*) noexcept=0;
    };
};
template<class D> struct produce<D,umbrella::explorer::layout_contract::Overrides> : produce_base<D,umbrella::explorer::layout_contract::Overrides> {
    int32_t __stdcall InitializeForContextCore(void* context) noexcept final {
        try {this->shim().initialize(umbrella::explorer::layout_contract::value(context));return 0;}catch(...){return to_hresult();}
    }
    int32_t __stdcall UninitializeForContextCore(void* context) noexcept final {
        try {this->shim().uninitialize(umbrella::explorer::layout_contract::value(context));return 0;}catch(...){return to_hresult();}
    }
    int32_t __stdcall MeasureOverride(void* context,Windows::Foundation::Size available,Windows::Foundation::Size* result) noexcept final {
        if(!result)return E_POINTER;
        *result={};try {*result=this->shim().measure(umbrella::explorer::layout_contract::value(context),available);return 0;}catch(...){return to_hresult();}
    }
    int32_t __stdcall ArrangeOverride(void* context,Windows::Foundation::Size finalSize,Windows::Foundation::Size* result) noexcept final {
        if(!result)return E_POINTER;
        *result={};try {*result=this->shim().arrange(umbrella::explorer::layout_contract::value(context),finalSize);return 0;}catch(...){return to_hresult();}
    }
    int32_t __stdcall OnItemsChangedCore(void* context,void* source,void* args) noexcept final {
        try {this->shim().itemsChanged(umbrella::explorer::layout_contract::value(context),umbrella::explorer::layout_contract::value(source),umbrella::explorer::layout_contract::value(args));return 0;}catch(...){return to_hresult();}
    }
};
}
namespace umbrella::explorer::layout_contract {
using Value=winrt::Windows::Foundation::IInspectable;
using Size=winrt::Windows::Foundation::Size;
inline constexpr GUID layoutId{0x24e50c1c,0x9c51,0x5144,{0x9d,0xdc,0x3f,0x50,0x01,0x91,0xc2,0x62}};
inline constexpr GUID protectedId{0xe46ff416,0x741e,0x5176,{0xa1,0xd4,0x57,0x18,0xd2,0x34,0x22,0x43}};
inline constexpr GUID contextId{0x283143f4,0x3716,0x5001,{0x90,0xad,0x10,0x79,0xf2,0x0d,0xf4,0xba}};
inline winrt::com_ptr<::IInspectable> contract(const Value& value,const GUID& id) {
    if(!value)winrt::throw_hresult(E_INVALIDARG);
    winrt::com_ptr<::IInspectable> result;
    winrt::check_hresult(reinterpret_cast<::IUnknown*>(winrt::get_abi(value))->QueryInterface(id,result.put_void()));
    return result;
}
inline void set(const Value& repeater,const Value& layout) {
    const auto element=repeater.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
    if(!element || !element.Dispatcher().HasThreadAccess())winrt::throw_hresult(RPC_E_WRONG_THREAD);
    if(!element.XamlRoot())winrt::throw_hresult(E_ABORT);
    const auto target=contract(repeater,repeater::itemsRepeaterId);
    using Put=HRESULT(STDMETHODCALLTYPE*)(void*,void*);
    winrt::check_hresult(reinterpret_cast<Put>((*reinterpret_cast<void***>(target.get()))[12])(target.get(),winrt::get_abi(layout)));
}
inline Size pass(const Value& layout,const Value& context,Size available,bool arrange) {
    const auto target=contract(layout,layoutId);
    using Call=HRESULT(STDMETHODCALLTYPE*)(void*,void*,Size,Size*);
    Size result{};
    winrt::check_hresult(reinterpret_cast<Call>((*reinterpret_cast<void***>(target.get()))[arrange?9:8])(target.get(),winrt::get_abi(context),available,&result));
    return result;
}
inline void contextLifetime(const Value& layout,const Value& context,bool initialize) {
    const auto target=contract(layout,layoutId);
    using Call=HRESULT(STDMETHODCALLTYPE*)(void*,void*);
    winrt::check_hresult(reinterpret_cast<Call>((*reinterpret_cast<void***>(target.get()))[initialize?6:7])(target.get(),winrt::get_abi(context)));
}
inline void invalidate(const Value& layout,bool measure) {
    const auto target=contract(layout,protectedId);
    using Call=HRESULT(STDMETHODCALLTYPE*)(void*);
    winrt::check_hresult(reinterpret_cast<Call>((*reinterpret_cast<void***>(target.get()))[measure?6:7])(target.get()));
}
using Invalidated=winrt::Windows::Foundation::TypedEventHandler<LayoutView,Value>;
inline winrt::event_token subscribe(const Value& layout,const Invalidated& callback,bool measure) {
    const auto target=contract(layout,layoutId);
    using Call=HRESULT(STDMETHODCALLTYPE*)(void*,void*,winrt::event_token*);
    winrt::event_token token{};
    winrt::check_hresult(reinterpret_cast<Call>((*reinterpret_cast<void***>(target.get()))[measure?10:12])(target.get(),winrt::get_abi(callback),&token));
    return token;
}
inline void unsubscribe(const Value& layout,winrt::event_token token,bool measure) {
    const auto target=contract(layout,layoutId);
    using Call=HRESULT(STDMETHODCALLTYPE*)(void*,winrt::event_token);
    winrt::check_hresult(reinterpret_cast<Call>((*reinterpret_cast<void***>(target.get()))[measure?11:13])(target.get(),token));
}
inline std::vector<winrt::Windows::UI::Xaml::UIElement> elements(const Value& context,unsigned limit=64) {
    const auto target=contract(context,contextId);
    auto** table=*reinterpret_cast<void***>(target.get());
    using Count=HRESULT(STDMETHODCALLTYPE*)(void*,int*);
    int count=0;winrt::check_hresult(reinterpret_cast<Count>(table[6])(target.get(),&count));
    if(count<0 || static_cast<unsigned>(count)>limit)winrt::throw_hresult(E_BOUNDS);
    using Get=HRESULT(STDMETHODCALLTYPE*)(void*,int,void**);
    std::vector<winrt::Windows::UI::Xaml::UIElement> result;
    result.reserve(count);
    for(int i=0;i<count;++i) {
        Value item{nullptr};
        winrt::check_hresult(reinterpret_cast<Get>(table[9])(target.get(),i,winrt::put_abi(item)));
        result.push_back(item.as<winrt::Windows::UI::Xaml::UIElement>());
    }
    return result;
}
// Owns real layout callbacks; callers never mutate foreign ItemsSource or the
// repeater's visual child collection. The original layout may be delegated to.
struct Attached : winrt::implements<Attached,Overrides,winrt::composing> {
    Value original{nullptr};
    std::function<Size(const Value&,Size)> measureCallback,arrangeCallback;
    unsigned measures=0,arranges=0,changes=0;
    winrt::event_token measureToken{},arrangeToken{};
    bool measureSubscribed=false,arrangeSubscribed=false,active=false;
    HRESULT failure=S_OK;
    winrt::hstring GetRuntimeClassName() const {return L"Microsoft.UI.Xaml.Controls.VirtualizingLayout";}
    Value create(const Value& factory) {
        constexpr GUID factoryId{0xc7c128e1,0x5df7,0x5345,{0xb5,0xcd,0x43,0xaf,0xdf,0xa9,0x48,0xb4}};
        const auto creating=contract(factory,factoryId);
        using Create=HRESULT(STDMETHODCALLTYPE*)(void*,void*,void**,void**);
        Value result{nullptr};
        auto outer=static_cast<Value>(*this);
        winrt::check_hresult(reinterpret_cast<Create>((*reinterpret_cast<void***>(creating.get()))[6])(
            creating.get(),winrt::get_abi(outer),winrt::put_abi(this->m_inner),winrt::put_abi(result)));
        return result;
    }
    void clearEvents() noexcept {
        active=false;
        for(bool measure:{true,false}) {
            auto& subscribed=measure ? measureSubscribed : arrangeSubscribed;
            if(!subscribed)continue;
            try {unsubscribe(original,measure?measureToken:arrangeToken,measure);subscribed=false;}
            catch(...) {
                // An unreleased delegate contains code from this DLL. Keep its
                // weak target safe and its image resident until process exit.
                HMODULE module{};
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                    reinterpret_cast<LPCWSTR>(&unsubscribe),&module);
            }
        }
    }
    ~Attached(){clearEvents();}
    void initialize(const Value& context) {
        if(!original)return;
        active=true;
        const auto weak=get_weak();
        for(bool measure:{true,false}) {
            const auto callback=Invalidated([weak,measure](const auto&,const auto&) {
                if(auto self=weak.get();self && self->active) {
                    try {invalidate(static_cast<Value>(*self),measure);}catch(...){self->failure=winrt::to_hresult();}
                }
            });
            if(measure){measureToken=subscribe(original,callback,true);measureSubscribed=true;}
            else {arrangeToken=subscribe(original,callback,false);arrangeSubscribed=true;}
        }
        contextLifetime(original,context,true);
    }
    void uninitialize(const Value& context) {clearEvents();if(original)contextLifetime(original,context,false);}
    Size measure(const Value& context,Size available) {
        ++measures;
        if(original)available=pass(original,context,available,false);
        try {return measureCallback ? measureCallback(context,available) : available;}
        catch(...) {failure=winrt::to_hresult();return available;}
    }
    Size arrange(const Value& context,Size finalSize) {
        ++arranges;
        if(original)finalSize=pass(original,context,finalSize,true);
        try {return arrangeCallback ? arrangeCallback(context,finalSize) : finalSize;}
        catch(...) {
            failure=winrt::to_hresult();
            return original ? pass(original,context,finalSize,true) : finalSize;
        }
    }
    void itemsChanged(const Value& context,const Value& source,const Value& args) {
        ++changes;
        if(original) {
            const auto overrides=original.as<Overrides>();
            winrt::check_hresult(reinterpret_cast<winrt::impl::abi_t<Overrides>*>(winrt::get_abi(overrides))->OnItemsChangedCore(winrt::get_abi(context),winrt::get_abi(source),winrt::get_abi(args)));
        }
        invalidate(static_cast<Value>(*this),true);
    }
};
}
