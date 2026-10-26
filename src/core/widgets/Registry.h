#pragma once
#include <array>
#include <string_view>

namespace umbrella::widgets {
struct Definition {
    std::string_view id, title, icon, category;
};
// Metadata only: no providers, factories or resources are started by registration.
inline constexpr std::array catalog{
    Definition{"clock", "Clock", "\xEE\x9E\x87", "Time"},
    Definition{"weather", "Weather", "\xEE\xA2\x9E", "Information"},
    Definition{"image", "Image", "\xEE\xAE\x9F", "Personal"},
    Definition{"user", "User", "\xEE\x9D\xBB", "Personal"},
    Definition{"cpu", "CPU", "\xEE\xA5\x90", "System"},
    Definition{"ram", "RAM", "\xEE\xA5\xA4", "System"},
    Definition{"temp", "Temp", "\xEE\xA7\x8A", "System"},
    Definition{"disk", "Disk", "\xEE\xB6\xA2", "System"},
    Definition{"battery", "Battery", "\xEE\xA1\x90", "System"},
    Definition{"github", "GitHub", "\xEE\xA5\x83", "Development"}
};
}
