#pragma once
#include "TaskbarContextMenu.h"
#include "TaskbarPanelOrder.h"
#include "core/taskbar/ComponentRegistry.h"
#include "core/taskbar/TrayOrder.h"
#include <winrt/Windows.UI.Xaml.Automation.h>

namespace umbrella::explorer::tray_order {
using namespace winrt::Windows::UI::Xaml;
class Session {
    panel_order::Session order_;
    static Controls::StackPanel find(HWND hwnd) {
        // Same exact loaded-image gates as the original Task View adapter.
        constexpr GUID ids[]{
            {0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}},
            {0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}},
            {0x2007f389,0x0b9d,0x4d16,{0xaf,0x78,0xba,0xfe,0x77,0x12,0x27,0x28}}};
        const wchar_t* dlls[]{L"Taskbar.dll",L"Taskbar.View.dll",L"SystemTray.dll"};
        for(unsigned i=0;i<3;++i)if(!native_layout::matches(GetModuleHandleW(dlls[i]),ids[i]))return nullptr;
        auto frame=context_menu::taskbarElement(hwnd);
        if(!frame || !frame.XamlRoot())return nullptr;
        std::vector<DependencyObject> pending{frame.XamlRoot().Content()};
        Controls::StackPanel found{nullptr}; unsigned visited=0;
        while(!pending.empty()) {
            if(++visited>512)return nullptr;
            auto node=pending.back();pending.pop_back();
            if(auto panel=node.try_as<Controls::StackPanel>(); panel && panel.Name()==L"SystemTrayFrameGrid") {
                if(found || panel.XamlRoot()!=frame.XamlRoot())return nullptr;
                found=panel;
            }
            const auto count=Media::VisualTreeHelper::GetChildrenCount(node);
            if(count<0 || pending.size()+count>512)return nullptr;
            for(int i=0;i<count;++i)pending.push_back(Media::VisualTreeHelper::GetChild(node,i));
        }
        return found;
    }
public:
    bool reset() noexcept{return order_.reset();}
    HRESULT apply(HWND hwnd,unsigned code) noexcept {
        // Keep the previously verified pair independent of language/overflow
        // visibility. More general orders require all four original groups.
        if(code<2)return clockBeforeQuickSettings(hwnd,code==1);
        const auto permutation=taskbar::trayOrder(code);
        if(!permutation)return E_INVALIDARG;
        try {
            if(!order_.bound() && !order_.bind(find(hwnd)))return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            auto desired=order_.original();
            std::array<size_t,4> slots;slots.fill(desired.size());
            for(size_t i=0;i<desired.size();++i) {
                auto element=desired[i].try_as<FrameworkElement>();if(!element)return E_ABORT;
                const auto type=winrt::get_class_name(element), name=element.Name();
                int role=-1;
                if(type==L"SystemTray.Stack" && name==L"NotifyIconStack")role=0;
                else if(type==L"SystemTray.Stack" && name==L"NonActivatableStack")role=1;
                else if(type==L"SystemTray.OmniButton" && name==L"ControlCenterButton")role=2;
                else if(type==L"SystemTray.OmniButton" && name==L"NotificationCenterButton")role=3;
                // Conditional groups retain their exact collection slots. They
                // participate in native layout and collision verification too.
                if(role<0)continue;
                if(slots[role]!=desired.size() || element.Visibility()!=Visibility::Visible ||
                    element.ActualWidth()<=0 || !element.IsHitTestVisible())return E_BOUNDS;
                slots[role]=i;
            }
            if(slots[3]>=desired.size() || !(slots[0]<slots[1] && slots[1]<slots[2] && slots[2]<slots[3]))return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            const auto original=desired;
            for(unsigned i=0;i<4;++i)desired[slots[i]]=original[slots[(*permutation)[i]]];
            const auto result=order_.apply(desired);
            switch(result) {
            case taskbar::Transaction::Applied:return S_OK;
            case taskbar::Transaction::Invalid:return E_INVALIDARG;
            case taskbar::Transaction::Stale:return HRESULT_FROM_WIN32(ERROR_RETRY);
            case taskbar::Transaction::RolledBack:return E_ABORT;
            default:return E_UNEXPECTED;
            }
        }catch(...){return winrt::to_hresult();}
    }
    HRESULT clockBeforeQuickSettings(HWND hwnd,bool enabled) noexcept {
        try {
            if(!enabled)return reset()?S_OK:E_ABORT;
            if(!order_.bound() && !order_.bind(find(hwnd)))return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            auto desired=order_.original();
            size_t clock=desired.size(),quick=desired.size();
            for(size_t i=0;i<desired.size();++i) {
                auto element=desired[i].try_as<FrameworkElement>();
                if(!element) return E_ABORT;
                const auto role=taskbar::identify(winrt::get_class_name(element).c_str(),element.Name().c_str(),Automation::AutomationProperties::GetAutomationId(element).c_str());
                if(role!=taskbar::Component::Clock && role!=taskbar::Component::QuickSettings)continue;
                if(element.Visibility()!=Visibility::Visible || element.ActualWidth()<=0 || !element.IsHitTestVisible())return E_ABORT;
                auto& index=role==taskbar::Component::Clock ? clock : quick;
                if(index!=desired.size())return E_ABORT;
                index=i;
            }
            // Preserve all unknown controls and the edge-specific Show Desktop.
            if(quick>=desired.size() || clock!=quick+1)return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            std::swap(desired[clock],desired[quick]);
            return order_.apply(desired)==taskbar::Transaction::Applied?S_OK:E_ABORT;
        }catch(...){return winrt::to_hresult();}
    }
};
}
