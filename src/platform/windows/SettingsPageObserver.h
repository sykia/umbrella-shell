#pragma once
#include <QtGlobal>
#include <QVariantMap>
#include <memory>

namespace umbrella::platform {
// Headless child mode. Only reads UIA; standard input EOF ends the observer.
int runSettingsPageObserver();
// One-shot, read-only diagnostic of an already open Settings window. No names/text.
int inspectSettingsPage();
class SettingsBlurIntegration final {
public:
    SettingsBlurIntegration();
    ~SettingsBlurIntegration();
    SettingsBlurIntegration(const SettingsBlurIntegration&) = delete;
    SettingsBlurIntegration& operator=(const SettingsBlurIntegration&) = delete;
    // Call on the GUI thread after acknowledged taskbar ownership changes.
    void update(bool enabled, bool managing);
    bool visible() const;
    QVariantMap diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
