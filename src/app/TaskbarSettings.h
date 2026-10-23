#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QSet>
#include "platform/windows/TaskbarModuleSession.h"
#include "platform/windows/TaskbarAppearance.h"

class TaskbarSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList layoutMonitors READ layoutMonitors NOTIFY changed)
    Q_PROPERTY(QVariantList appearanceMonitors READ appearanceMonitors NOTIFY changed)
    Q_PROPERTY(QString selectedAppearanceMonitor READ selectedAppearanceMonitor NOTIFY changed)
    Q_PROPERTY(QVariantMap activeComponentConfiguration READ activeComponentConfiguration NOTIFY changed)
    Q_PROPERTY(QString activeComponentMonitor READ activeComponentMonitor NOTIFY changed)
    Q_PROPERTY(QVariantMap activeComponentConfigurations READ activeComponentConfigurations NOTIFY changed)
    Q_PROPERTY(bool componentPlacementAvailable READ sidesAvailable CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool widthAvailable READ widthAvailable CONSTANT)
    Q_PROPERTY(bool sidesAvailable READ sidesAvailable CONSTANT)
    Q_PROPERTY(bool autoHide READ autoHide NOTIFY changed)
    Q_PROPERTY(QString edge READ edge NOTIFY changed)
    Q_PROPERTY(bool detached READ detached NOTIFY changed)
    Q_PROPERTY(int widthPercent READ widthPercent NOTIFY changed)
    Q_PROPERTY(int thickness READ thickness NOTIFY changed)
    Q_PROPERTY(bool islands READ islands NOTIFY changed)
    Q_PROPERTY(int transparency READ transparency NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit TaskbarSettings(QObject* parent = nullptr);
    bool restoreSavedAppearance(bool freshSession = true);
    bool savedAppearanceRestored() const { return restored_; }
    QVariantList appearanceMonitors() const;
    QString selectedAppearanceMonitor() const { return selectedAppearanceMonitor_; }
    Q_INVOKABLE void selectAppearanceMonitor(const QString& id);
    bool available() const { return available_; }
    bool widthAvailable() const;
    bool sidesAvailable() const;
    bool autoHide() const { return autoHide_; }
    QString edge() const { return edge_; }
    bool detached() const { return detached_; }
    int widthPercent() const { return widthPercent_; }
    int thickness() const { return thickness_; }
    bool islands() const { return islands_; }
    int transparency() const { return transparency_; }
    bool pending() const { return pending_; }
    QString status() const { return status_; }
    QVariantList layoutMonitors() const;
    QVariantMap activeComponentConfiguration() const { return activeComponentConfiguration_; }
    QString activeComponentMonitor() const { return activeComponentMonitor_; }
    QVariantMap activeComponentConfigurations() const { return activeComponents_; }
    Q_INVOKABLE QStringList componentIds(const QString& monitor,bool refresh=false);
    Q_INVOKABLE bool applyComponentConfiguration(const QString& monitor, const QVariantMap& configuration);
    Q_INVOKABLE bool restoreComponentConfiguration(const QString& monitor);
    Q_INVOKABLE QVariantMap componentDraft(const QString& monitor) const;
    Q_INVOKABLE bool saveComponentDraft(const QString& monitor, const QVariantMap& draft);
    Q_INVOKABLE bool placeTaskViewAfterWidgets(const QString& monitor, int gap);
    Q_INVOKABLE bool resetComponentLayout();
    Q_INVOKABLE void applyAutoHide(bool enabled);
    Q_INVOKABLE void apply(const QString& edge, bool detached, int transparency, bool autoHide, int widthPercent, int thickness, bool islands);
    Q_INVOKABLE void resetAppearance();
signals:
    void changed();
private:
    bool restoreSavedAppearanceImpl(bool freshSession);
    quint64 resumeEpoch_ = 0;
    bool applyNativeComponents(const QString& monitor,const QVariantMap& configuration);
    bool resetNativeComponents(const QString& monitor);
    bool persistConfirmedComponents();
    bool replayConfirmedComponents();
    QString selectedAppearanceMonitor_;
    bool restored_ = false;
    QVariantMap activeComponentConfiguration_;
    QString activeComponentMonitor_;
    QVariantMap activeComponents_,confirmedComponents_;
    QSet<QString> appearanceComponents_,legacyTrayComponents_;
    QHash<QString,QStringList> componentCatalogue_;
    bool available_ = false, autoHide_ = false, pending_ = false;
    QString status_;
    QString edge_ = "bottom";
    bool detached_ = false;
    int transparency_ = 35;
    int widthPercent_ = 100;
    int thickness_ = 48;
    bool islands_ = false;
    umbrella::platform::TaskbarAppearance appearance_;
    umbrella::platform::TaskbarModuleSession componentSession_;
};
