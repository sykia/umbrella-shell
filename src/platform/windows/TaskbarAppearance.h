#pragma once
#include <QString>
#include <memory>

namespace umbrella::platform {
class TaskbarAppearance final {
public:
    TaskbarAppearance();
    ~TaskbarAppearance();
    TaskbarAppearance(const TaskbarAppearance&) = delete;
    TaskbarAppearance& operator=(const TaskbarAppearance&) = delete;
    bool apply(const QString& edge, bool detached, int transparency, int widthPercent, int thickness, bool islands, QString& error);
    bool restore(QString& error);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
int watchTaskbarAppearance(unsigned long parent, const QString& journal, const QString& stopEvent);
bool recoverTaskbarAppearance();
}
