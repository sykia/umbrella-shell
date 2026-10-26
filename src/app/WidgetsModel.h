#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <memory>

class QWindow;
namespace umbrella::platform { class DesktopLayer; }

class WidgetsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY monitorsChanged)
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QVariantList catalog READ catalog CONSTANT)
    Q_PROPERTY(QString selectedMonitorId READ selectedMonitorId NOTIFY editChanged)
    Q_PROPERTY(QString selectedMonitorLabel READ selectedMonitorLabel NOTIFY editChanged)
    Q_PROPERTY(QVariantMap selectedClock READ selectedClock NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY editChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int desktopRevision READ desktopRevision NOTIFY desktopChanged)
public:
    explicit WidgetsModel(QObject* parent = nullptr);
    ~WidgetsModel() override;
    QVariantList catalog() const;
    QVariantList monitors() const;
    QVariantList instances() const;
    QVariantMap selectedClock() const;
    QString selectedMonitorId() const { return monitorId_; }
    QString selectedMonitorLabel() const;
    QString selectedId() const { return selectedId_; }
    QString status() const { return status_; }
    bool editing() const { return editing_; }
    int desktopRevision() const { return desktopRevision_; }
    Q_INVOKABLE void openEditor(const QString& monitorId = {});
    Q_INVOKABLE void closeEditor();
    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void selectInstance(const QString& id);
    Q_INVOKABLE bool addClock(const QString& mode);
    Q_INVOKABLE bool updateClock(const QVariantMap& settings);
    Q_INVOKABLE bool moveInstance(const QString& id, double globalX, double globalY);
    Q_INVOKABLE bool removeInstance(const QString& id);
    Q_INVOKABLE bool attachDesktop(QWindow* window, const QString& id);
    Q_INVOKABLE bool prepareWindow(QWindow* window, bool transparency, bool dark);
signals:
    void monitorsChanged();
    void instancesChanged();
    void editChanged();
    void selectionChanged();
    void statusChanged();
    void desktopChanged();
private:
    void refreshMonitors();
    bool save();
    bool validSettings(const QVariantMap& settings) const;
    QSettings settings_{QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Widgets"};
    std::unique_ptr<umbrella::platform::DesktopLayer> desktop_;
    QVariantList monitors_;
    QVariantList layout_;
    QString monitorId_;
    QString selectedId_;
    QString selectedInstanceId_;
    QString status_;
    bool editing_ = false;
    int desktopRevision_ = 0;
};
