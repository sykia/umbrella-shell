#pragma once

namespace umbrella::updates {
enum class Channel { Stable, Beta };
enum class InstallDecision { Disabled, VerificationRequired, ConsentRequired, RecoveryRequired, Allowed };

// Policy only. Booleans must eventually come from trusted adapters; they are
// not signature verification, authorization tokens or a security boundary.
struct InstallPrerequisites {
    bool integrationEnabled = false;
    bool packageVerified = false;
    bool userConsented = false;
    bool recoveryReady = false;
};

[[nodiscard]] constexpr InstallDecision evaluateInstall(InstallPrerequisites p) noexcept {
    if (!p.integrationEnabled) return InstallDecision::Disabled;
    if (!p.packageVerified) return InstallDecision::VerificationRequired;
    if (!p.userConsented) return InstallDecision::ConsentRequired;
    if (!p.recoveryReady) return InstallDecision::RecoveryRequired;
    return InstallDecision::Allowed;
}

[[nodiscard]] constexpr bool accepts(Channel channel, bool prerelease, bool draft) noexcept {
    return !draft && (channel == Channel::Beta || !prerelease);
}
}
