#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QRect>
#include <functional>
#include <memory>
class QWindow;
class QScreen;
namespace umbrella::platform {
// Explorer's desktop hierarchy is undocumented: accept only verified layouts.
class DesktopLayer final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit DesktopLayer(QObject* parent = nullptr);
    ~DesktopLayer() override;
    bool place(QWindow* window, QScreen* screen, const QRect& geometry);
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
    std::function<void()> changed;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
