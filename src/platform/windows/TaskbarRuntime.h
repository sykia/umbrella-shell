#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QVariantList>
#include <QStringList>
#include <functional>
#include <memory>

namespace umbrella::platform {
// Owns the per-user/session instance lock and event-driven Explorer readiness.
class TaskbarRuntime final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit TaskbarRuntime(QObject* parent = nullptr);
    ~TaskbarRuntime() override;
    bool ownsInstance() const;
    bool start(std::function<bool()> ready);
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
    static QVariantList monitors();
    static QStringList panelOwners();
    static bool setLoginResume(bool enabled);
    static QString reportPath();
    static bool writeRestoreReport(bool succeeded, const QString& status);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
