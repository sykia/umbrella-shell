#pragma once
#include <system_error>

namespace umbrella::platform {
// Opens the system Explorer. Never modifies Winlogon, policies or autostart.
[[nodiscard]] std::error_code openExplorer();
}
