#pragma once
#include "core/taskbar/ModelOrder.h"
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Core.h>

namespace umbrella::explorer::panel_order {
using namespace winrt::Windows::UI::Xaml;
// Logical collection order, not post-layout Translation. Caller establishes
// ownership of this static StackPanel; never use for ItemsRepeater children.
class Session {
    Controls::StackPanel panel_{nullptr};
    std::vector<UIElement> original_, confirmed_;
    bool safe_=true;
    struct Adapter {
        Controls::StackPanel panel;
        bool snapshot(std::vector<UIElement>& out) const {
            if (!panel || !panel.Dispatcher().HasThreadAccess() || !panel.XamlRoot()) return false;
            const auto children=panel.Children(); const auto count=children.Size();
            if (count>taskbar::maxReorderItems) return false;
            out.clear(); out.reserve(count);
            for(unsigned i=0;i<count;++i) out.push_back(children.GetAt(i));
            return children.Size()==count;
        }
        bool remove(unsigned i) const { panel.Children().RemoveAt(i); return true; }
        bool insert(unsigned i,const UIElement& item) const { panel.Children().InsertAt(i,item); return true; }
        bool verify(const std::vector<UIElement>& order) const {
            panel.UpdateLayout();
            std::vector<UIElement> actual;
            if (!snapshot(actual) || actual!=order) return false;
            std::vector<taskbar::Rectangle> boxes;
            for(const auto& item : order) {
                auto element=item.try_as<FrameworkElement>();
                if (!element || Media::VisualTreeHelper::GetParent(item)!=panel || element.XamlRoot()!=panel.XamlRoot()) return false;
                if (element.Visibility()!=Visibility::Visible || element.ActualWidth()<=0 || element.ActualHeight()<=0) continue;
                const auto box=element.TransformToVisual(panel).TransformBounds({0,0,static_cast<float>(element.ActualWidth()),static_cast<float>(element.ActualHeight())});
                const taskbar::Rectangle measured{box.X,box.Y,box.Width,box.Height};
                if (!taskbar::inside(measured,panel.ActualWidth(),panel.ActualHeight())) return false;
                for(const auto& other : boxes) if(taskbar::intersects(measured,other)) return false;
                boxes.push_back(measured);
            }
            return true;
        }
    };
public:
    Session()=default;
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    ~Session(){reset();}
    bool bound() const {return panel_!=nullptr;}
    bool bind(const Controls::StackPanel& panel) noexcept {
        if (panel_) return panel_==panel;
        try {
            if (!panel || panel.Orientation()!=Controls::Orientation::Horizontal) return false;
            Adapter adapter{panel}; std::vector<UIElement> items;
            if (!adapter.snapshot(items) || items.size()<2 || !adapter.verify(items)) return false;
            original_=items; confirmed_=items; panel_=panel; safe_=true; return true;
        } catch (...) {return false;}
    }
    const std::vector<UIElement>& original() const {return original_;}
    taskbar::Transaction apply(const std::vector<UIElement>& order) noexcept {
        if(!panel_ || !safe_)return taskbar::Transaction::Stale;
        try {
            Adapter adapter{panel_};
            const auto result=taskbar::reorderModel(confirmed_,order,adapter);
            if(result==taskbar::Transaction::Applied) confirmed_=order;
            if(result==taskbar::Transaction::RollbackFailed || result==taskbar::Transaction::Stale) safe_=false;
            return result;
        }catch(...){safe_=false;return taskbar::Transaction::RollbackFailed;}
    }
    bool reset() noexcept {
        if(!panel_)return true;
        try {
            Adapter adapter{panel_}; std::vector<UIElement> actual;
            // Never overwrite a newer system collection after invalidation.
            if(!adapter.snapshot(actual)) {panel_=nullptr;original_.clear();confirmed_.clear();return false;}
            if(actual!=original_ && (!safe_ || taskbar::reorderModel(confirmed_,original_,adapter)!=taskbar::Transaction::Applied))return false;
            panel_=nullptr; original_.clear(); confirmed_.clear(); safe_=true; return true;
        }catch(...){return false;}
    }
};
}
