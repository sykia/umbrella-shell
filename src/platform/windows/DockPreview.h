#pragma once
#include "DockWindows.h"
#include <QColor>
#include <QRectF>
class QWindow;
namespace umbrella::platform {
// Lazy, non-activating native popup. DWM owns live image composition; no capture loop.
class DockPreview final : public QObject {
    Q_OBJECT
public:
    explicit DockPreview(QObject* parent=nullptr);
    ~DockPreview() override;
    bool show(QWindow& owner, const QRectF& localAnchor, const QString& edge,
        const QString& title, const QList<DockWindow>& windows,
        QColor background, QColor foreground, QColor accent);
    void hide();
    bool visible() const;
signals:
    void hovered(bool inside);
    void activated(const umbrella::platform::DockWindow& window);
    void closeRequested(const umbrella::platform::DockWindow& window);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
