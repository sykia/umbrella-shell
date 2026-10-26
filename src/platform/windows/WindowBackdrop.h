#pragma once

class QWindow;

namespace umbrella::platform {
// Default: documented Windows Mica. acrylic=true: clear native blur compatibility
// path, then system Acrylic/opaque fallbacks. Call on theme/effects changes.
bool applyWindowBackdrop(QWindow& window, bool enabled = true, bool dark = false, bool acrylic = false);
}
