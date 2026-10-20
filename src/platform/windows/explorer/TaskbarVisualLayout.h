#pragma once
#include <array>
#include <memory>
#include <vector>

namespace umbrella::explorer::visual_layout {
using namespace winrt::Windows::UI::Xaml;
using Value = winrt::Windows::Foundation::IInspectable;
struct Property { FrameworkElement element{nullptr}; DependencyProperty property{nullptr}; Value original{nullptr}; };
struct Override { FrameworkElement element{nullptr}; DependencyProperty property{nullptr}; int64_t token{}; };
struct Control { bool active = true; bool writing = false; };
// Feed the cross-axis frame size into Explorer's native size cache so Explorer
// owns the per-monitor work area. Button sizes remain native.
class Session {
    std::array<Property, 8> originals_{};
    std::array<Override, 6> overrides_{};
    unsigned originalCount_ = 0, overrideCount_ = 0;
    FrameworkElement frame_{nullptr}, tray_{nullptr}, background_{nullptr};
    std::array<FrameworkElement, 2> strokes_{nullptr, nullptr};
    std::shared_ptr<Control> control_;
    winrt::event_token sizeToken_{};
    bool sizeSubscribed_ = false;
    void remember(const FrameworkElement& element, const DependencyProperty& property) {
        originals_[originalCount_++] = {element, property, element.ReadLocalValue(property)};
    }
    void set(const FrameworkElement& element, const DependencyProperty& property, const Value& desired) {
        auto& entry = overrides_[overrideCount_]; entry.element = element; entry.property = property;
        const auto control = control_;
        entry.token = element.RegisterPropertyChangedCallback(property, [control, desired](auto&& object, auto&& changed) {
            if (!control->active || control->writing) return;
            control->writing = true;
            try { object.SetValue(changed, desired); } catch (...) {}
            control->writing = false;
        });
        ++overrideCount_;
        control->writing = true; element.SetValue(property, desired); control->writing = false;
    }
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() { reset(); }
    bool prepare(HWND hwnd) noexcept {
        if (frame_ && tray_ && background_) return true;
        try {
            frame_ = context_menu::taskbarElement(hwnd);
            if (!frame_ || !frame_.XamlRoot()) { frame_ = nullptr; return false; }
            std::vector<DependencyObject> stack{frame_.XamlRoot().Content()};
            unsigned visited = 0;
            while (!stack.empty() && visited++ < 512) {
                auto node = stack.back(); stack.pop_back();
                if (auto element = node.try_as<FrameworkElement>()) {
                    const auto type = winrt::get_class_name(node), name = element.Name();
                    if (type == L"SystemTray.SystemTrayFrame") { tray_ = element; continue; }
                    if (name == L"TaskbarFrameRepeater") continue;
                    if (type == L"Taskbar.TaskbarBackground" && name == L"BackgroundControl") background_ = element;
                    if (name == L"BackgroundStroke") strokes_[0] = element;
                    if (name == L"ScreenEdgeStroke") strokes_[1] = element;
                }
                const int count = Media::VisualTreeHelper::GetChildrenCount(node);
                for (int i = 0; i < count && stack.size() < 512; ++i) stack.push_back(Media::VisualTreeHelper::GetChild(node, i));
            }
            if (!tray_ || !background_) { reset(); return false; }
            remember(frame_, FrameworkElement::WidthProperty()); remember(frame_, FrameworkElement::HeightProperty());
            remember(background_, UIElement::RenderTransformProperty()); remember(background_, UIElement::RenderTransformOriginProperty());
            remember(tray_, FrameworkElement::HorizontalAlignmentProperty()); remember(tray_, FrameworkElement::VerticalAlignmentProperty());
            for (const auto& stroke : strokes_) if (stroke) remember(stroke, UIElement::OpacityProperty());
            return true;
        } catch (...) { reset(); return false; }
    }
    void suspend() noexcept {
        if (control_) control_->active = false;
        if (sizeSubscribed_) { try { background_.SizeChanged(sizeToken_); } catch (...) {} sizeSubscribed_ = false; }
        const unsigned modified = overrideCount_;
        for (unsigned i = 0; i < modified; ++i) {
            auto& entry = overrides_[i];
            try { entry.element.UnregisterPropertyChangedCallback(entry.property, entry.token); } catch (...) {}
        }
        overrideCount_ = 0; control_.reset();
        for (unsigned i = 0; i < modified; ++i) {
            const auto& changed = overrides_[i];
            for (unsigned j = 0; j < originalCount_; ++j) {
                const auto& saved = originals_[j];
                if (saved.element != changed.element || saved.property != changed.property) continue;
                try {
                    if (saved.original == DependencyProperty::UnsetValue()) saved.element.ClearValue(saved.property);
                    else saved.element.SetValue(saved.property, saved.original);
                } catch (...) {}
                break;
            }
            overrides_[i] = {};
        }
    }
    void reset() noexcept {
        suspend();
        for (auto& entry : originals_) entry = {};
        originalCount_ = 0; frame_ = nullptr; tray_ = nullptr; background_ = nullptr;
        strokes_ = {nullptr, nullptr};
    }
    bool apply(HWND hwnd, unsigned edge, unsigned thickness) noexcept {
        wchar_t name[64]{};
        if (GetClassNameW(hwnd, name, 64) && !wcscmp(name, L"Umbrella.Taskbar.ModuleFixture.1")) return true;
        suspend();
        if (thickness == 48) return true;
        if (!prepare(hwnd)) return false;
        try {
            control_ = std::make_shared<Control>();
            const bool vertical = edge == 0 || edge == 2;
            // A HWND resize or a backdrop transform alone leaves Explorer's
            // reserved strip unchanged. Its XAML frame size notification updates
            // the native reservation (including secondary taskbars). Save both
            // axes before the first override; suspend restores them before an
            // edge switch, reset, or owner-death teardown.
            set(frame_, vertical ? FrameworkElement::WidthProperty() : FrameworkElement::HeightProperty(),
                winrt::box_value(static_cast<double>(thickness)));
            Media::ScaleTransform scale;
            const auto update = [scale, background = background_, vertical, thickness] {
                const double original = vertical ? background.ActualWidth() : background.ActualHeight();
                const double factor = static_cast<double>(thickness) / (original > 0 ? original : 48.0);
                if (vertical) scale.ScaleX(factor); else scale.ScaleY(factor);
            };
            update();
            Media::TransformGroup transform;
            transform.Children().Append(background_.RenderTransform()); transform.Children().Append(scale);
            set(background_, UIElement::RenderTransformOriginProperty(), winrt::box_value(winrt::Windows::Foundation::Point{0.5f,0.5f}));
            set(background_, UIElement::RenderTransformProperty(), transform);
            if (vertical) set(tray_, FrameworkElement::HorizontalAlignmentProperty(), winrt::box_value(HorizontalAlignment::Center));
            else set(tray_, FrameworkElement::VerticalAlignmentProperty(), winrt::box_value(VerticalAlignment::Center));
            for (const auto& stroke : strokes_) if (stroke) set(stroke, UIElement::OpacityProperty(), winrt::box_value(0.0));
            const auto control = control_;
            sizeToken_ = background_.SizeChanged([control, update](auto&&, auto&&) { if (control->active) { try { update(); } catch (...) {} } });
            sizeSubscribed_ = true;
            tray_.UpdateLayout();
            return true;
        } catch (...) { suspend(); return false; }
    }
};
}
