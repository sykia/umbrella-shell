#pragma once

namespace umbrella::taskbar {
// Persist before entering native code. Only a surviving owner may replenish it.
// Invalid persisted values fail closed rather than granting extra attempts.
inline int nextResumeAttempt(int previous) noexcept {
    return previous >= 0 && previous < 2 ? previous + 1 : 0;
}
}
