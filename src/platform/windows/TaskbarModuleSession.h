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
    bool apply(int percent, const QString& edge, int thickness, bool islands, bool detached, QString& error, const QString& monitor = {});
    bool stop(QString& error);
    bool positionTaskView(int x, QString& error, bool afterWidgets = false);
private:
    friend struct TaskbarModuleSessionTest;
    bool waitForLine(const QByteArray& expected);
    QProcess host_;
};
}
