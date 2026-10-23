#pragma once
#include <QString>
#include <memory>
#include <functional>
class QObject;

namespace umbrella::platform {
class TaskbarAppearance final {
public:
    TaskbarAppearance();
    ~TaskbarAppearance();
    TaskbarAppearance(const TaskbarAppearance&) = delete;
    TaskbarAppearance& operator=(const TaskbarAppearance&) = delete;
    bool apply(const QString& edge, bool detached, int transparency, int widthPercent, int thickness, bool islands, QString& error);
    bool applyMonitor(const QString& device, const QString& edge, bool detached, int transparency, int widthPercent, int thickness, bool islands, QString& error);
    bool restore(QString& error);
    bool nativeSessionActive() const;
    bool active() const;
    void onStateChanged(QObject* context, std::function<void()> callback);
    bool reorderTray(const QString& device,unsigned order,QString& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
int watchTaskbarAppearance(unsigned long parent, const QString& journal, const QString& stopEvent);
bool recoverTaskbarAppearance();
}
