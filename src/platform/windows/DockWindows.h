#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QImage>
#include <QRect>
#include <QVariantMap>
#include <memory>
class QWindow;

namespace umbrella::platform {
struct DockApp {
    QString id, title, path, executable, aumid;
    QImage icon;
};
struct DockWindow {
    DockApp app;
    quintptr handle = 0;
    quint32 processId = 0;
    quint64 processCreated = 0;
    QString title, screen;
    QRect rect;
    QRect monitorRect;
    bool active = false, fullscreen = false;
};
class DockWindows final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit DockWindows(QObject* parent = nullptr);
    ~DockWindows() override;
    bool start();
    void stop();
    QList<DockWindow> windows() const;
    void requestSnapshot();
    QList<DockApp> importPins() const;
    static DockApp application(const QString& path);
    static bool launch(const DockApp& app);
    static bool activate(const DockWindow& window, bool minimizeActive);
    static bool close(const DockWindow& window);
    static QVariantMap theme();
    static bool backdrop(QWindow& window, bool enabled, bool dark);
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
signals:
    void windowsChanged();
    void themeChanged();
    void snapshotReady(const QList<umbrella::platform::DockWindow>& windows);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool snapshotPending_ = false;
    bool snapshotAgain_ = false;
    unsigned generation_ = 0;
};
}
