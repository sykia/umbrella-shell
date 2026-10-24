#pragma once
#include <QProcess>
#include <QString>
#include <QHash>
#include <QQueue>
#include <QStringList>
#include <functional>

namespace umbrella::platform {
class TaskbarModuleSession final {
public:
    TaskbarModuleSession();
    ~TaskbarModuleSession();
    static bool available();
    static bool layoutAvailable();
    static QHash<QString,QStringList> discoverComponents(QString& error,const QHash<QString,QStringList>& retainedHidden={});
    bool start(QString& error);
    bool apply(int percent, const QString& edge, int thickness, bool islands, bool detached, QString& error, const QString& monitor = {});
    bool stop(QString& error);
    bool positionTaskView(int x, QString& error, bool afterWidgets = false);
    bool configureComponents(bool taskViewLeft,int gap,unsigned trayOrder,QString& error);
    bool running() const {return host_.state()!=QProcess::NotRunning;}
    bool reorderTray(const QString& device,unsigned order,QString& error);
    bool placeComponents(const QByteArray& command,const QString& device,QString& error);
    bool previousPlacementPreserved() const {return placementPreserved_;}
    std::function<void(const QString&)> componentLost;
private:
    friend struct TaskbarModuleSessionTest;
    static QHash<QString,QStringList> decodeComponents(const QByteArray& output,const QHash<QString,QStringList>& retainedHidden);
    bool waitForLine(const QByteArray& expected);
    void readOutput();
    void deferLoss(const QString& device,unsigned serial);
    QProcess host_;
    QQueue<QByteArray> replies_;
    QHash<QString,unsigned> placements_;
    bool stopping_=false;
    unsigned processEpoch_=0;
    QString placementFailure_;
    bool placementPreserved_=false;
};
}
