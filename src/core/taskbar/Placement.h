#pragma once
#include <cmath>
#include <span>

namespace umbrella::taskbar {
struct Rectangle { double x{}, y{}, width{}, height{}; };
inline bool valid(Rectangle r) noexcept {
    return std::isfinite(r.x) && std::isfinite(r.y) &&
        std::isfinite(r.width) && std::isfinite(r.height) && r.width > 0 && r.height > 0 &&
        std::isfinite(r.x + r.width) && std::isfinite(r.y + r.height);
}
inline bool intersects(Rectangle a, Rectangle b) noexcept {
    // Invalid measurements must fail closed, including collapsed/stale peers.
    if (!valid(a) || !valid(b)) return true;
    constexpr double tolerance = 0.5; // XAML subpixel rounding, DIP.
    return a.x < b.x + b.width - tolerance && a.x + a.width > b.x + tolerance &&
        a.y < b.y + b.height - tolerance && a.y + a.height > b.y + tolerance;
}
inline bool inside(Rectangle r, double width, double height) noexcept {
    return valid(r) && std::isfinite(width) && std::isfinite(height) &&
        width > 0 && height > 0 && r.x >= -0.5 && r.y >= -0.5 &&
        r.x + r.width <= width + 0.5 && r.y + r.height <= height + 0.5;
}
inline bool verified(Rectangle r, double requestedX, double width, double height,
                     std::span<const Rectangle> obstacles) noexcept {
    if (!std::isfinite(requestedX) || !inside(r, width, height) || std::abs(r.x - requestedX) > 0.5) return false;
    for (const auto obstacle : obstacles) if (intersects(r, obstacle)) return false;
    return true;
}
}
