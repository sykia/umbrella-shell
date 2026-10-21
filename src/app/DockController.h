#pragma once
#include "platform/windows/DockWindows.h"
#include "platform/windows/DockPreview.h"
#include <QAbstractListModel>
#include <QVariantMap>
#include <QQuickView>
#include <QTimer>
#include <vector>

class DockController final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY screensChanged)
public:
    explicit DockController(QObject* parent = nullptr);
    ~DockController() override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex&, int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    bool enabled() const { return enabled_; }
    QVariantMap config() const { return config_; }
    QVariantMap theme() const { return theme_; }
    QString status() const { return status_; }
    QVariantList screens() const;
    void attachEngine(QQmlEngine& engine);
    void restore();
    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE void configure(const QString& key, const QVariant& value);
    Q_INVOKABLE QVariantMap screenConfig(const QString& name) const;
    Q_INVOKABLE void configureScreen(const QString& name, const QString& key, const QVariant& value);
    Q_INVOKABLE void importWindowsPins();
    Q_INVOKABLE bool addApplication(const QString& path);
    Q_INVOKABLE void pin(int row);
    Q_INVOKABLE void movePin(int from, int to);
    Q_INVOKABLE void sortPins();
    Q_INVOKABLE void resetPinOrder();
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE int indexOf(const QString& identity) const;
    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE void activateWindow(const QString& handle);
    Q_INVOKABLE void closeWindow(const QString& handle);
    Q_INVOKABLE void interaction(const QString& screen, bool hovered, bool menu);
    Q_INVOKABLE void requestPreview(const QString& identity,const QString& screen,qreal x,qreal y,qreal width,qreal height);
    Q_INVOKABLE void cancelPreview(const QString& identity);
    Q_INVOKABLE void dismissPreview();
    Q_INVOKABLE void showSettings(bool dockPage = false) { emit settingsRequested(dockPage); }
    Q_INVOKABLE void quit() { emit quitRequested(); }
signals:
    void enabledChanged();
    void configChanged();
    void themeChanged();
    void statusChanged();
    void screensChanged();
    void settingsRequested(bool dockPage);
    void quitRequested();
private:
    friend struct DockFixture;
    struct Entry { umbrella::platform::DockApp app; QList<umbrella::platform::DockWindow> windows; bool pinned = false; };
    struct Surface;
    enum Role { Identity = Qt::UserRole+1, Title, Icon, Pinned, Count, Active, Windows };
    QList<Entry> entries_;
    QList<umbrella::platform::DockWindow> windowSnapshot_;
    QList<umbrella::platform::DockApp> pins_;
    umbrella::platform::DockWindows source_;
    umbrella::platform::DockPreview preview_;
    QVariantMap config_, theme_, monitorConfig_;
    QHash<QString,QString> screenIds_;
    QString status_;
    bool enabled_ = false;
    bool rebuilding_ = false;
    QTimer refreshTimer_;
    QTimer persistenceTimer_;
    QTimer previewShowTimer_,previewHideTimer_;
    QString previewIdentity_,previewScreen_;
    QRectF previewAnchor_;
    bool previewHovered_=false;
    std::vector<std::unique_ptr<Surface>> surfaces_;
    void refresh();
    void reconcile(const QList<umbrella::platform::DockWindow>& windows);
    void rebuildSurfaces();
    void layoutSurfaces();
    void save();
    void report(const QString& text);
    void refreshScreenIds();
    QString screenKey(const QString& name) const;
    void showPendingPreview();
};
