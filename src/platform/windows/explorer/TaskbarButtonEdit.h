#pragma once
#include "TaskbarContextMenu.h"
#include "TaskbarModuleProtocol.h"
#include "core/taskbar/Placement.h"
#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace umbrella::explorer::button_edit {
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using Value = winrt::Windows::Foundation::IInspectable;
// All XAML access and teardown run on the existing taskbar UI thread.
class Session {
    struct Control {
        FrameworkElement button{nullptr}, item{nullptr}, parent{nullptr}, inputRoot{nullptr};
        Value savedHit{nullptr};
        bool hitMasked = false;
        winrt::Windows::Foundation::Numerics::float3 savedTranslation{}, writtenTranslation{};
        double offset{};
        bool editing = false, dragging = false, live = false, wrote = false;
        double pointerStart{}, offsetStart{}, committed{};
        unsigned moves{}, clicks{}, focusEvents{};
        winrt::hstring blockerType;
        double blockerX{}, blockerWidth{};
    };
    struct Handler { RoutedEvent event{nullptr}; Value callback{nullptr}; bool attached = false; };
    std::shared_ptr<Control> control_;
    std::array<Handler, 5> handlers_{};
    winrt::event_token unloaded_{}, layout_{}, click_{}, focus_{};
    bool unloadedSet_ = false, layoutSet_ = false, clickSet_ = false, focusSet_ = false;

    static void pin() noexcept {
        HMODULE module{};
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&pin), &module);
    }
    static void unmask(const std::shared_ptr<Control>& c) {
        if (c->hitMasked) {
            if (!c->item.IsHitTestVisible()) {
                if (c->savedHit == DependencyProperty::UnsetValue()) c->item.ClearValue(UIElement::IsHitTestVisibleProperty());
                else c->item.SetValue(UIElement::IsHitTestVisibleProperty(), c->savedHit);
            }
            c->hitMasked = false;
        }
    }
    static bool restore(const std::shared_ptr<Control>& c) noexcept {
        c->editing = false; c->dragging = false; c->live = false;
        try {
            c->inputRoot.ReleasePointerCaptures();
            unmask(c);
            if (c->wrote) {
                // Do not overwrite a newer native value (e.g. template recreation).
                if (c->item.Translation() == c->writtenTranslation) c->item.Translation(c->savedTranslation);
                c->offset = 0;
                c->wrote = false;
            }
            c->committed = 0;
            return true;
        } catch (...) { pin(); return false; }
    }
    static bool valid(const std::shared_ptr<Control>& c) {
        return c->live && c->item.XamlRoot() && c->item.XamlRoot() == c->parent.XamlRoot() &&
            Media::VisualTreeHelper::GetParent(c->item) == c->parent &&
            c->parent.ActualWidth() > c->item.ActualWidth() &&
            (!c->wrote || c->item.Translation() == c->writtenTranslation);
    }
    static double baseX(const std::shared_ptr<Control>& c) {
        const auto p = c->item.TransformToVisual(c->parent).TransformPoint({0, 0});
        return p.X - c->offset;
    }
    static bool place(const std::shared_ptr<Control>& c, double offset) {
        if (!valid(c) || !std::isfinite(offset)) return false;
        const double x = baseX(c);
        const double limit = c->parent.ActualWidth() - c->item.ActualWidth();
        if (!std::isfinite(x) || !std::isfinite(limit) || limit < 0) return false;
        const double clamped = std::clamp(x + offset, 0.0, limit) - x;
        if (c->wrote && std::abs(clamped - c->offset) < 0.01) return true;
        // XAML's Translation facade maintains hit testing, without replacing
        // RenderTransform used by native pointer/theme animations.
        c->writtenTranslation = c->savedTranslation;
        c->writtenTranslation.x += static_cast<float>(clamped);
        c->wrote = true; // Arm rollback before the first property write.
        c->item.Translation(c->writtenTranslation);
        c->offset = clamped;
        ++c->moves;
        return true;
    }
    static bool overlaps(const std::shared_ptr<Control>& c) {
        const auto rectangle = c->item.TransformToVisual(c->parent).TransformBounds(
            {0, 0, static_cast<float>(c->item.ActualWidth()), static_cast<float>(c->item.ActualHeight())});
        const taskbar::Rectangle current{rectangle.X, rectangle.Y, rectangle.Width, rectangle.Height};
        if (!taskbar::inside(current, c->parent.ActualWidth(), c->parent.ActualHeight())) return true;
        const int count = Media::VisualTreeHelper::GetChildrenCount(c->parent);
        if (count < 0 || count > 128) return true;
        for (int i = 0; i < count; ++i) {
            auto other = Media::VisualTreeHelper::GetChild(c->parent, i).try_as<FrameworkElement>();
            if (!other || other == c->item || other.Visibility() != Visibility::Visible ||
                other.ActualWidth() <= 0 || other.ActualHeight() <= 0) continue;
            const auto r = other.TransformToVisual(c->parent).TransformBounds(
                {0, 0, static_cast<float>(other.ActualWidth()), static_cast<float>(other.ActualHeight())});
            if (taskbar::intersects(current, {r.X, r.Y, r.Width, r.Height})) {
                c->blockerType = winrt::get_class_name(other); c->blockerX = r.X; c->blockerWidth = r.Width;
                return true;
            }
        }
        return false;
    }
    static void finish(const std::shared_ptr<Control>& c, bool commit) {
        c->dragging = false;
        if (!commit || !valid(c) || overlaps(c)) { if (!place(c, c->offsetStart)) restore(c); }
        c->committed = c->offset;
    }
    static std::shared_ptr<Control> find(HWND hwnd) {
        wchar_t name[64]{};
        if (!GetClassNameW(hwnd, name, 64) || wcscmp(name, L"Shell_TrayWnd")) return {};
        // No feature-cache or registry writes. Check all loaded taskbar images.
        constexpr GUID ids[]{
            {0xdab69a79,0xfee0,0x33f0,{0xd8,0x1e,0x02,0x3f,0xbf,0x17,0x2b,0x51}},
            {0x24534974,0xe00e,0x46a5,{0x8f,0x69,0xf6,0x0d,0x0c,0xeb,0x9b,0xd9}},
            {0x2007f389,0x0b9d,0x4d16,{0xaf,0x78,0xba,0xfe,0x77,0x12,0x27,0x28}}
        };
        const wchar_t* dlls[]{L"Taskbar.dll", L"Taskbar.View.dll", L"SystemTray.dll"};
        for (unsigned i = 0; i < 3; ++i) if (!native_layout::matches(GetModuleHandleW(dlls[i]), ids[i])) return {};
        auto frame = context_menu::taskbarElement(hwnd);
        if (!frame || !frame.XamlRoot()) return {};
        auto c = std::make_shared<Control>();
        std::vector<DependencyObject> stack{frame};
        unsigned visited = 0, matches = 0;
        while (!stack.empty()) {
            if (++visited > 512) return {};
            auto node = stack.back(); stack.pop_back();
            if (auto element = node.try_as<FrameworkElement>()) {
                if (Automation::AutomationProperties::GetAutomationId(element) == L"TaskViewButton" || element.Name() == L"TaskViewButton") {
                    c->button = element; ++matches;
                }
            }
            const int count = Media::VisualTreeHelper::GetChildrenCount(node);
            if (count < 0 || stack.size() + count > 512) return {};
            for (int i = 0; i < count; ++i) stack.push_back(Media::VisualTreeHelper::GetChild(node, i));
        }
        if (matches != 1) return {};
        c->item = c->button;
        for (unsigned i = 0; i < 8; ++i) {
            auto parent = Media::VisualTreeHelper::GetParent(c->item).try_as<FrameworkElement>();
            if (!parent) return {};
            if (parent.Name() == L"TaskbarFrameRepeater") { c->parent = parent; break; }
            c->item = parent;
        }
        if (!c->parent) return {};
        return c;
    }
    template<typename Callback> void handler(unsigned index, const RoutedEvent& event, Callback callback) {
        auto& h = handlers_[index];
        h.event = event; h.callback = winrt::box_value(PointerEventHandler(callback));
        // Mark before the call so a partially completed registration is removed too.
        h.attached = true;
        control_->inputRoot.AddHandler(h.event, h.callback, true);
    }
public:
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() { reset(); }
    bool reset() noexcept {
        auto c = control_;
        if (!c) return true;
        c->live = false;
        bool ok = restore(c);
        for (auto& h : handlers_) if (h.attached) {
            try { c->inputRoot.RemoveHandler(h.event, h.callback); h.attached = false; } catch (...) { ok = false; }
        }
        try { if (unloadedSet_) { c->item.Unloaded(unloaded_); unloadedSet_ = false; } } catch (...) { ok = false; }
        try { if (layoutSet_) { c->item.LayoutUpdated(layout_); layoutSet_ = false; } } catch (...) { ok = false; }
        try { if (clickSet_) { c->button.as<Controls::Primitives::ButtonBase>().Click(click_); clickSet_ = false; } } catch (...) { ok = false; }
        try { if (focusSet_) { c->button.GotFocus(focus_); focusSet_ = false; } } catch (...) { ok = false; }
        if (!ok) { pin(); return false; } // Inert shared callbacks remain valid even if teardown fails.
        control_.reset();
        for (auto& h : handlers_) h = {};
        return true;
    }
    HRESULT begin(HWND hwnd, bool editor = true) noexcept {
        if (!reset()) return E_FAIL;
        try {
            auto c = find(hwnd);
            if (!c || !c->button.try_as<Controls::Primitives::ButtonBase>() ||
                c->item.ActualWidth() <= 0 || c->item.ActualWidth() > 80 ||
                c->item.ActualHeight() <= 0 || c->parent.ActualWidth() <= c->item.ActualWidth()) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            // Do not move a composite item containing any other interactive button.
            std::vector<DependencyObject> nodes{c->item};
            unsigned buttons = 0, visited = 0;
            while (!nodes.empty()) {
                if (++visited > 128) return E_ABORT;
                auto node = nodes.back(); nodes.pop_back();
                if (node.try_as<Controls::Primitives::ButtonBase>()) ++buttons;
                const int count = Media::VisualTreeHelper::GetChildrenCount(node);
                for (int i = 0; i < count && nodes.size() < 128; ++i) nodes.push_back(Media::VisualTreeHelper::GetChild(node, i));
            }
            if (buttons != 1) return E_ABORT;
            c->inputRoot = Media::VisualTreeHelper::GetParent(c->parent).try_as<Controls::Grid>();
            if (!c->inputRoot || c->inputRoot.Name() != L"RootGrid" || !c->inputRoot.as<Controls::Panel>().Background()) return E_ABORT;
            if (!c->item.IsHitTestVisible()) return E_ABORT;
            c->savedHit = c->item.ReadLocalValue(UIElement::IsHitTestVisibleProperty());
            if (c->savedHit != DependencyProperty::UnsetValue() && !c->savedHit.try_as<winrt::Windows::Foundation::IReference<bool>>()) return E_ABORT;
            // Do not combine a developer drag with native Translation animations.
            if (c->item.TranslationTransition()) return E_ABORT;
            c->savedTranslation = c->item.Translation();
            if (!std::isfinite(c->savedTranslation.x) || !std::isfinite(c->savedTranslation.y) || !std::isfinite(c->savedTranslation.z)) return E_ABORT;
            c->editing = editor; c->live = true;
            control_ = c;
            if (editor) {
            handler(0, UIElement::PointerPressedEvent(), [c](auto&&, const PointerRoutedEventArgs& e) {
                try {
                    if (!c->editing || !valid(c)) return;
                    const auto point = e.GetCurrentPoint(c->parent);
                    if (point.PointerDevice().PointerDeviceType() != winrt::Windows::Devices::Input::PointerDeviceType::Mouse ||
                        !point.Properties().IsLeftButtonPressed() || point.Properties().IsRightButtonPressed()) return;
                    const auto box = c->item.TransformToVisual(c->parent).TransformBounds(
                        {0, 0, static_cast<float>(c->item.ActualWidth()), static_cast<float>(c->item.ActualHeight())});
                    if (point.Position().X < box.X || point.Position().X >= box.X + box.Width ||
                        point.Position().Y < box.Y || point.Position().Y >= box.Y + box.Height) return;
                    c->pointerStart = point.Position().X;
                    c->offsetStart = c->committed;
                    c->dragging = c->inputRoot.CapturePointer(e.Pointer());
                    e.Handled(c->dragging);
                } catch (...) { restore(c); }
            });
            handler(1, UIElement::PointerMovedEvent(), [c](auto&&, const PointerRoutedEventArgs& e) {
                try {
                    if (!c->dragging) return;
                    e.Handled(true);
                    if (!place(c, c->offsetStart + e.GetCurrentPoint(c->parent).Position().X - c->pointerStart)) restore(c);
                } catch (...) { restore(c); }
            });
            handler(2, UIElement::PointerReleasedEvent(), [c](auto&&, const PointerRoutedEventArgs& e) {
                try {
                    if (!c->dragging) return;
                    e.Handled(true);
                    place(c, c->offsetStart + e.GetCurrentPoint(c->parent).Position().X - c->pointerStart);
                    finish(c, true);
                    c->inputRoot.ReleasePointerCapture(e.Pointer());
                } catch (...) { restore(c); }
            });
            auto cancel = [c](auto&&, const PointerRoutedEventArgs&) {
                if (!c->dragging) return;
                try { finish(c, false); c->inputRoot.ReleasePointerCaptures(); }
                catch (...) { restore(c); }
            };
            handler(3, UIElement::PointerCanceledEvent(), cancel);
            handler(4, UIElement::PointerCaptureLostEvent(), [c](auto&&, const PointerRoutedEventArgs& e) {
                if (!c->dragging) return;
                try {
                    // ButtonBase releases capture in its class handler, before our
                    // PointerReleased handler. A physical button-up is a drop, not cancellation.
                    const auto p = e.GetCurrentPoint(c->parent);
                    const bool released = !p.Properties().IsLeftButtonPressed();
                    if (released) place(c, c->offsetStart + p.Position().X - c->pointerStart);
                    finish(c, released);
                } catch (...) { restore(c); }
            });
            }
            unloaded_ = c->item.Unloaded([c](auto&&, auto&&) { restore(c); }); unloadedSet_ = true;
            layout_ = c->item.LayoutUpdated([c](auto&&, auto&&) {
                if (!c->live || !c->wrote) return;
                try {
                    const auto p = c->item.TransformToVisual(c->parent).TransformPoint({0, 0});
                    if (!valid(c) || p.X < -0.5f || p.X + c->item.ActualWidth() > c->parent.ActualWidth() + 0.5 ||
                        (!c->dragging && overlaps(c))) restore(c);
                } catch (...) { restore(c); }
            }); layoutSet_ = true;
            click_ = c->button.as<Controls::Primitives::ButtonBase>().Click([c](auto&&, auto&&) { if (c->live) ++c->clicks; }); clickSet_ = true;
            focus_ = c->button.GotFocus([c](auto&&, auto&&) { if (c->live) ++c->focusEvents; }); focusSet_ = true;
            // Edit input belongs to the existing XAML root, not to the native
            // ButtonBase class handler. The original visual stays in place.
            if (editor) { c->hitMasked = true; c->item.IsHitTestVisible(false); }
            return S_OK;
        } catch (...) { const auto hr = winrt::to_hresult(); reset(); return hr; }
    }
    HRESULT position(HWND hwnd, double x) noexcept {
        try {
            if (!std::isfinite(x)) return E_INVALIDARG;
            // Rebind once on an explicit request after Unloaded/layout invalidation.
            // No timer or repeated visual-tree scans are needed while dragging.
            if (!control_ || !valid(control_)) { const auto hr = begin(hwnd, false); if (FAILED(hr)) return hr; }
            auto c = control_;
            if (c->editing || c->dragging || x < 0 || x + c->item.ActualWidth() > c->parent.ActualWidth()) return E_INVALIDARG;
            const double previous = c->offset;
            const double previousX = baseX(c) + previous;
            const bool placed = place(c, x - baseX(c));
            const auto actual = c->item.TransformToVisual(c->parent).TransformBounds(
                {0, 0, static_cast<float>(c->item.ActualWidth()), static_cast<float>(c->item.ActualHeight())});
            if (!placed || !valid(c) || !c->item.IsHitTestVisible() ||
                !taskbar::verified({actual.X, actual.Y, actual.Width, actual.Height}, x,
                    c->parent.ActualWidth(), c->parent.ActualHeight(), {}) || overlaps(c)) {
                // A successful property setter is not proof of successful placement.
                if (!place(c, previous) || std::abs(baseX(c) + c->offset - previousX) > 0.5 || overlaps(c)) restore(c);
                return HRESULT_FROM_WIN32(ERROR_BUSY);
            }
            c->committed = c->offset;
            return S_OK;
        } catch (...) { const auto hr = winrt::to_hresult(); reset(); return hr; }
    }
    HRESULT afterWidgets(HWND hwnd, double gap) noexcept {
        try {
            if (!std::isfinite(gap) || gap < 0 || gap > 96) return E_INVALIDARG;
            if (!control_ || !valid(control_)) { const auto hr=begin(hwnd,false); if (FAILED(hr)) return hr; }
            auto c=control_;
            if (!valid(c)) return E_ABORT;
            const int count=Media::VisualTreeHelper::GetChildrenCount(c->parent);
            if (count > 128) return E_ABORT;
            double end=0; unsigned matches=0;
            for (int i=0;i<count;++i) {
                auto node=Media::VisualTreeHelper::GetChild(c->parent,i).try_as<FrameworkElement>();
                if (!node || winrt::get_class_name(node) != L"Taskbar.AugmentedEntryPointButton" || node.Visibility()!=Visibility::Visible || node.ActualWidth()<=0) continue;
                const auto r=node.TransformToVisual(c->parent).TransformBounds({0,0,static_cast<float>(node.ActualWidth()),static_cast<float>(node.ActualHeight())});
                end=r.X+r.Width; ++matches;
            }
            return matches==1 ? position(hwnd,end+gap) : HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        } catch (...) { const auto hr=winrt::to_hresult(); reset(); return hr; }
    }
    bool preview() noexcept {
        if (!control_) return true;
        auto c = control_;
        try {
            if (c->dragging) finish(c, true);
            c->editing = false; c->dragging = false;
            c->inputRoot.ReleasePointerCaptures(); unmask(c);
            return true;
        } catch (...) { restore(c); return false; }
    }
    EditReport report(HWND hwnd, HRESULT status) noexcept {
        EditReport r; r.status = status;
        try {
            auto c = control_ ? control_ : find(hwnd);
            if (!c) { if (SUCCEEDED(status)) r.status = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED); return r; }
            if (control_ && (!valid(c) || (c->wrote && !c->dragging && overlaps(c)))) {
                restore(c);
                if (SUCCEEDED(r.status)) r.status = HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
            }
            r.editing = c->editing; r.dragging = c->dragging; r.moves = c->moves; r.clicks = c->clicks; r.focusEvents = c->focusEvents;
            r.hitTestVisible = c->item.IsHitTestVisible();
            if (auto grid = Media::VisualTreeHelper::GetParent(c->parent).try_as<Controls::Panel>()) r.rootBackground = grid.Background() ? 1u : 0u;
            r.offset = c->offset;
            wcsncpy_s(r.blockerType, c->blockerType.c_str(), _TRUNCATE); r.blockerX = c->blockerX; r.blockerWidth = c->blockerWidth;
            const auto p = c->button.TransformToVisual(c->parent).TransformPoint({0, 0});
            r.buttonX = p.X; r.buttonY = p.Y; r.buttonWidth = c->button.ActualWidth(); r.parentWidth = c->parent.ActualWidth();
            DependencyObject node = c->button;
            for (; node && r.ancestors < 8; node = Media::VisualTreeHelper::GetParent(node)) {
                auto& a = r.chain[r.ancestors++];
                wcsncpy_s(a.type, winrt::get_class_name(node).c_str(), _TRUNCATE);
                if (auto f = node.try_as<FrameworkElement>()) {
                    wcsncpy_s(a.name, f.Name().c_str(), _TRUNCATE); a.width = f.ActualWidth(); a.height = f.ActualHeight();
                }
            }
        } catch (...) { r.status = winrt::to_hresult(); }
        return r;
    }
};
}
