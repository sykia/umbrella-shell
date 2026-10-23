#pragma once
#include "TaskbarModuleProtocol.h"
#include "TaskbarRepeaterAccess.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace umbrella::explorer::repeater {
struct SourceCapabilities {
    HRESULT status=E_INVALIDARG;
    std::uint32_t contracts{};
    bool liveIndexed() const noexcept {
        return SUCCEEDED(status) && (contracts&(SourceVector|SourceVectorView|SourceBindableVector)) &&
            (contracts&(SourceNotifyCollection|SourceObservableVector|SourceBindableObservable));
    }
};
// Six bounded QIs, no enumeration, mutation, event registration or private ABI.
inline SourceCapabilities sourceCapabilities(const Value& source) noexcept {
    SourceCapabilities result;
    if(!source)return result;
    result.status=S_OK;
    const auto query=[&]<class Interface>(SourceContract flag) {
        winrt::com_ptr<::IUnknown> contract;
        const auto status=reinterpret_cast<::IUnknown*>(winrt::get_abi(source))->QueryInterface(winrt::guid_of<Interface>(),contract.put_void());
        if(status==E_NOINTERFACE)return true;
        if(status!=S_OK || !contract){result.status=FAILED(status)?status:E_UNEXPECTED;result.contracts=0;return false;}
        result.contracts|=flag;return true;
    };
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Windows::UI::Xaml::Interop;
    if(!query.template operator()<IVector<Value>>(SourceVector) ||
        !query.template operator()<IVectorView<Value>>(SourceVectorView) ||
        !query.template operator()<IBindableVector>(SourceBindableVector) ||
        !query.template operator()<INotifyCollectionChanged>(SourceNotifyCollection) ||
        !query.template operator()<IObservableVector<Value>>(SourceObservableVector) ||
        !query.template operator()<IBindableObservableVector>(SourceBindableObservable))return result;
    return result;
}
}
