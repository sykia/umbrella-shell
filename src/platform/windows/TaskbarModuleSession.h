#pragma once
#include <QProcess>
#include <QString>

namespace umbrella::platform {
class TaskbarModuleSession final {
public:
    ~TaskbarModuleSession();
    static bool available();
    static bool layoutAvailable();
    bool start(QString& error);
    bool apply(int percent, const QString& edge, int thickness, bool islands, bool detached, QString& error);
    bool stop(QString& error);
private:
    bool waitForLine(const QByteArray& expected);
    QProcess host_;
};
}
