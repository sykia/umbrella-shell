#pragma once
#include <QProcess>
#include <QString>
#include <functional>

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
    bool configureComponents(bool taskViewLeft,int gap,unsigned trayOrder,QString& error);
    bool running() const {return host_.state()!=QProcess::NotRunning;}
    bool active() const { return applied_ && host_.state() == QProcess::Running; }
    void onStateChanged(QObject* context, std::function<void()> callback);
    bool reorderTray(const QString& device,unsigned order,QString& error);
private:
    bool applied_ = false;
    friend struct TaskbarModuleSessionTest;
    bool waitForLine(const QByteArray& expected);
    QProcess host_;
};
}
