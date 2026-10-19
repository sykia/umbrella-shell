#pragma once

class QWindow;

namespace umbrella::platform {
// Applies the untinted system blur-behind to this window only. False keeps the opaque QML fallback.
bool applyWindowBackdrop(QWindow& window);
}
