#include "core/updates/UpdatePolicy.h"
#include <initializer_list>
#include <iostream>

int main() {
    using namespace umbrella::updates;
    int failures = 0;
    // Exhaust the prerequisite truth table, including Release builds (no assert).
    for (unsigned mask = 0; mask < 16; ++mask) {
        const InstallPrerequisites p{bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)};
        if ((evaluateInstall(p) == InstallDecision::Allowed) != (mask == 15)) ++failures;
    }
    // Ordered refusals keep the UI from asking consent for an untrusted package.
    if (evaluateInstall({false, true, true, true}) != InstallDecision::Disabled) ++failures;
    if (evaluateInstall({true, false, false, false}) != InstallDecision::VerificationRequired) ++failures;
    if (evaluateInstall({true, true, false, false}) != InstallDecision::ConsentRequired) ++failures;
    if (evaluateInstall({true, true, true, false}) != InstallDecision::RecoveryRequired) ++failures;
    for (auto channel : {Channel::Stable, Channel::Beta}) {
        if (accepts(channel, false, true) || accepts(channel, true, true)) ++failures;
        if (!accepts(channel, false, false)) ++failures;
    }
    if (accepts(Channel::Stable, true, false) || !accepts(Channel::Beta, true, false)) ++failures;
    std::cout << "Policy failures: " << failures << '\n';
    return failures ? 1 : 0;
}
