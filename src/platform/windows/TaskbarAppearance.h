#pragma once
#include <QString>
#include <memory>
#include <functional>

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
    bool reorderTray(const QString& device,unsigned order,QString& error);
    bool placeComponents(const QByteArray& command,const QString& device,QString& error);
    void setComponentLossHandler(std::function<void(const QString&)> handler);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
int watchTaskbarAppearance(unsigned long parent, const QString& journal, const QString& stopEvent);
bool recoverTaskbarAppearance();
}
