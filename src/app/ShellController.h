#pragma once
#include <QObject>
#include <QSettings>
#include <QQuickView>
#include <memory>
#include <vector>
#include "platform/windows/DesktopBar.h"
#include "platform/windows/TaskbarSession.h"
#include "platform/windows/PanelActions.h"
#include "platform/windows/MonitorWorkspaces.h"
class ShellController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool replacement READ replacement WRITE setReplacement NOTIFY changed)
    Q_PROPERTY(bool startup READ startup WRITE setStartup NOTIFY changed)
    Q_PROPERTY(QString panelEdge READ panelEdge WRITE setPanelEdge NOTIFY changed)
    Q_PROPERTY(QString keyboardLayout READ keyboardLayout NOTIFY changed)
    Q_PROPERTY(QVariantMap moduleZones READ moduleZones NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit ShellController(QObject* parent = nullptr);
    ~ShellController() override;
    bool start();
    bool replacement() const { return enabled_; }
    bool startup() const;
    QString status() const { return status_; }
    QString panelEdge() const { return edge_; }
    void setPanelEdge(const QString& edge);
    QString keyboardLayout() const { return actions_.layout(); }
    QVariantMap moduleZones() const { return zones_; }
    Q_INVOKABLE QVariantList workspaceModel(const QString& panel) const;
    Q_INVOKABLE void selectWorkspace(const QString& panel, int index);
    Q_INVOKABLE void setModuleZone(const QString& module, int zone);
    Q_INVOKABLE void toggleSettings();
    Q_INVOKABLE void cycleKeyboard();
    Q_INVOKABLE void openSound();
    Q_INVOKABLE void moveDesktop(int direction);
    Q_INVOKABLE void showDesktops();
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
    bool registerBars();
    void restore();
    void fail(const QString& error);
    umbrella::platform::BarEdge nativeEdge() const;
    QSettings settings_;
    umbrella::platform::PanelActions actions_;
    umbrella::platform::MonitorWorkspaces workspaces_;
    QString edge_ = "top";
    QVariantMap zones_{{"workspaces", 0}, {"keyboard", 0}, {"volume", 2}};
    std::vector<std::unique_ptr<Bar>> bars_;
    std::unique_ptr<QQuickView> settingsView_;
    umbrella::platform::TaskbarSession session_;
    bool enabled_ = true;
    bool active_ = false;
    bool rebuilding_ = false;
    QString status_;
};
