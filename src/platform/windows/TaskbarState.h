#pragma once
#include <optional>

namespace umbrella::platform {
std::optional<bool> taskbarAutoHide();
// Requests a change; callers must read the state again to confirm it.
bool requestTaskbarAutoHide(bool enabled);
}
