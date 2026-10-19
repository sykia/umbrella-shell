#pragma once
#include <d2d1.h>
#include <windows.graphics.interop.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <wrl/client.h>
#include <array>

namespace umbrella::explorer::smooth_clip {
using Microsoft::WRL::ComPtr;
using Interop = ABI::Windows::Graphics::IGeometrySource2DInterop;
struct Source : winrt::implements<Source, winrt::Windows::Graphics::IGeometrySource2D, Interop> {
    std::array<D2D1_ROUNDED_RECT, maxIslands> rectangles{};
    unsigned count{};
    ComPtr<ID2D1Factory> factory;
    Source(const RECT* bounds, unsigned size, float dpi, float radius) : count(size) {
        winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, factory.GetAddressOf()));
        const float unit = 96.0f / dpi;
        for (unsigned i = 0; i < count; ++i) {
            const auto& r = bounds[i];
            rectangles[i] = {{r.left * unit, r.top * unit, r.right * unit, r.bottom * unit}, radius, radius};
        }
    }
    HRESULT create(ID2D1Factory* target, ID2D1Geometry** result) noexcept {
        if (!target || !result) return E_POINTER;
        *result = nullptr;
        std::array<ComPtr<ID2D1RoundedRectangleGeometry>, maxIslands> owned;
        std::array<ID2D1Geometry*, maxIslands> raw{};
        for (unsigned i = 0; i < count; ++i) {
            HRESULT hr = target->CreateRoundedRectangleGeometry(rectangles[i], &owned[i]);
            if (FAILED(hr)) return hr;
            raw[i] = owned[i].Get();
        }
        ComPtr<ID2D1GeometryGroup> group;
        HRESULT hr = target->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, raw.data(), count, &group);
        if (FAILED(hr)) return hr;
        *result = group.Detach();
        return S_OK;
    }
    HRESULT __stdcall GetGeometry(ID2D1Geometry** result) noexcept final { return create(factory.Get(), result); }
    HRESULT __stdcall TryGetGeometryUsingFactory(ID2D1Factory* target, ID2D1Geometry** result) noexcept final { return create(target, result); }
};
class Session {
    winrt::Windows::UI::Composition::Visual visual_{nullptr};
    winrt::Windows::UI::Composition::CompositionClip original_{nullptr};
    winrt::Windows::UI::Composition::CompositionBorderMode border_{};
public:
    ~Session() { reset(); }
    Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    void reset() noexcept {
        if (visual_) {
            try { visual_.Clip(original_); visual_.BorderMode(border_); } catch (...) {}
        }
        visual_ = nullptr; original_ = nullptr;
    }
    bool apply(HWND hwnd, const RECT* rectangles, unsigned count, float radius) noexcept {
        if (!count || count > maxIslands) return false;
        wchar_t name[64]{};
        if (GetClassNameW(hwnd, name, 64) && !wcscmp(name, L"Umbrella.Taskbar.ModuleFixture.1")) return true;
        try {
            if (!visual_) {
                const auto frame = context_menu::taskbarElement(hwnd);
                if (!frame || !frame.XamlRoot()) return false;
                visual_ = winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(frame.XamlRoot().Content());
                original_ = visual_.Clip(); border_ = visual_.BorderMode();
            }
            const float dpi = static_cast<float>(GetDpiForWindow(hwnd));
            if (dpi <= 0) return false;
            auto source = winrt::make<Source>(rectangles, count, dpi, radius);
            const auto compositor = visual_.Compositor();
            const auto geometry = compositor.CreatePathGeometry(winrt::Windows::UI::Composition::CompositionPath(source));
            visual_.BorderMode(winrt::Windows::UI::Composition::CompositionBorderMode::Soft);
            visual_.Clip(compositor.CreateGeometricClip(geometry));
            return true;
        } catch (...) { reset(); return false; }
    }
};
}
