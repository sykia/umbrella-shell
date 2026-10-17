#pragma once
#include <QObject>
#include <QSettings>
#include <QQuickView>
#include <memory>
#include <vector>
#include "platform/windows/DesktopBar.h"
#include "platform/windows/TaskbarSession.h"
class ShellController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool replacement READ replacement WRITE setReplacement NOTIFY changed)
    Q_PROPERTY(bool startup READ startup WRITE setStartup NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit ShellController(QObject* parent = nullptr);
    ~ShellController() override;
    bool start();
    bool replacement() const { return enabled_; }
    bool startup() const;
    QString status() const { return status_; }
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void quit();
    void setReplacement(bool enabled);
    void setStartup(bool enabled);
signals:
    void changed();
private:
    struct Bar {
        std::unique_ptr<QQuickView> view;
        std::unique_ptr<umbrella::platform::DesktopBar> native;
        bool painted = false;
    };
    void rebuild();
    void activate();
    void restore();
    void fail(const QString& error);
    QSettings settings_;
    std::vector<std::unique_ptr<Bar>> bars_;
    std::unique_ptr<QQuickView> settingsView_;
    umbrella::platform::TaskbarSession session_;
    bool enabled_ = true;
    bool active_ = false;
    bool rebuilding_ = false;
    QString status_;
};
