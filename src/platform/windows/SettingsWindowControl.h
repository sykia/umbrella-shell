#pragma once
#include <QAbstractNativeEventFilter>
class QGuiApplication;
class QWindow;
namespace umbrella::platform {
bool activateExistingSettingsWindow();
class SettingsWindowControl final : public QAbstractNativeEventFilter {
    QGuiApplication& app_;
    QWindow& window_;
public:
    SettingsWindowControl(QGuiApplication& app, QWindow& window);
    ~SettingsWindowControl() override;
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;
};
}
