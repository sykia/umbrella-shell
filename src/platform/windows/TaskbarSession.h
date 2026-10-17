#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <memory>
namespace umbrella::platform {
// Owns native session resources. No Windows types escape this adapter.
class TaskbarSession final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit TaskbarSession(QObject* parent = nullptr);
    ~TaskbarSession() override;
    bool acquire();
    bool hotkeyAvailable() const;
    bool prepare();
    bool hide();
    bool restore();
    bool originalAutoHide() const;
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
signals:
    void recoveryRequested();
    void sessionEnding();
    void explorerRestarted();
    void positionChanged(quintptr window);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
