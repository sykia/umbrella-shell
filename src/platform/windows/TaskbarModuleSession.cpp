#include "TaskbarModuleSession.h"
#include "explorer/NativeTaskbarLayout.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <Windows.h>

namespace umbrella::platform {
void TaskbarModuleSession::onStateChanged(QObject* context, std::function<void()> callback) {
    QObject::connect(&host_, &QProcess::stateChanged, context, [callback](QProcess::ProcessState) { callback(); });
}
namespace {
QString hostPath() { return QCoreApplication::applicationDirPath() + "/UmbrellaTaskbarHost.exe"; }
}
bool TaskbarModuleSession::available() {
    return QFileInfo::exists(hostPath()) && QFileInfo::exists(QCoreApplication::applicationDirPath() + "/UmbrellaTaskbarModule.dll");
}
bool TaskbarModuleSession::layoutAvailable() { return available() && explorer::native_layout::installedBuildSupported(); }
TaskbarModuleSession::~TaskbarModuleSession() { QString ignored; stop(ignored); }
bool TaskbarModuleSession::waitForLine(const QByteArray& expected) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 3000) {
        while (host_.canReadLine()) {
            const auto line = host_.readLine(512).trimmed();
            if (line == expected) return true;
            if (line.startsWith("error ") || line.startsWith("FAILED") || line == "taskview-refused" || line.startsWith("tray-refused ")) return false;
        }
        if (!host_.waitForReadyRead(static_cast<int>(3000 - elapsed.elapsed()))) return false;
    }
    return false;
}
bool TaskbarModuleSession::start(QString& error) {
    if (host_.state() == QProcess::Running) return true;
    if (!available()) {
        error = QStringLiteral("Модуль меню панели задач не найден рядом с приложением.");
        return false;
    }
    host_.setProgram(hostPath());
    host_.setArguments({"--serve"});
    host_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    host_.start();
    if (host_.waitForStarted(3000) && waitForLine("ready")) return true;
    QString ignored;
    stop(ignored);
    error = QStringLiteral("Модуль меню Explorer не подтвердил подключение.");
    return false;
}
bool TaskbarModuleSession::apply(int percent, const QString& edge, int thickness, bool islands, bool detached, QString& error, const QString& monitor) {
    if (percent < 40 || percent > 100 || thickness < 48 || thickness > 96) { error = QStringLiteral("Недопустимая ширина панели."); return false; }
    const int location = edge == "left" ? 0 : edge == "top" ? 1 : edge == "right" ? 2 : edge == "bottom" ? 3 : -1;
    if (location < 0) { error = QStringLiteral("Недопустимое расположение панели."); return false; }
    if (host_.state() == QProcess::NotRunning) {
        if (!start(error)) return false;
    }
    auto value = QByteArray::number(percent) + ' ' + QByteArray::number(location) + ' ' + QByteArray::number(thickness) + ' ' + QByteArray::number(islands ? 1 : 0) + ' ' + QByteArray::number(detached ? 1 : 0);
    if (!monitor.isEmpty()) {
        if (!monitor.startsWith(QStringLiteral("\\\\.\\DISPLAY")) || monitor.size() > 32 || monitor.contains(' ') || monitor.contains('\n')) { error = QStringLiteral("Invalid monitor device."); return false; }
        value += ' ' + monitor.toLatin1();
    }
    if (host_.write(value + '\n') != value.size() + 1 || !host_.waitForBytesWritten(3000) || !waitForLine("applied " + value)) {
        QString ignored; stop(ignored);
        error = QStringLiteral("Модуль Explorer не подтвердил ширину панели."); return false;
    }
    applied_ = true;
    return true;
}
bool TaskbarModuleSession::positionTaskView(int x, QString& error, bool afterWidgets) {
    if (x < 0 || x > (afterWidgets ? 96 : 16384) || !layoutAvailable()) { error = QStringLiteral("Task View: сборка DLL или позиция не поддерживается."); return false; }
    if (host_.state() == QProcess::NotRunning) {
        host_.setProgram(hostPath()); host_.setArguments({"--configure-taskview"});
        host_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
        host_.start();
        if (!host_.waitForStarted(3000) || !waitForLine("layout-ready")) {
            QString ignored; stop(ignored); error = QStringLiteral("Task View: нужна исходная нижняя панель, 100%, 48 DIP, DPI 100%, без автоскрытия и другой native-сессии."); return false;
        }
    }
    const auto command = QByteArray(afterWidgets ? "after-widgets " : "position ") + QByteArray::number(x) + '\n';
    if (host_.write(command) != command.size() || !host_.waitForBytesWritten(3000) || !waitForLine("taskview-ok")) {
        QString ignored; stop(ignored); error = QStringLiteral("Task View: Explorer отказал (занятое место или несовместимое состояние). Запрошен откат."); return false;
    }
    applied_ = true;
    return true;
}
bool TaskbarModuleSession::stop(QString& error) {
    applied_ = false;
    if (host_.state() == QProcess::NotRunning) return true;
    host_.closeWriteChannel(); // EOF restores the subclass and original rectangle, then exits.
    if (!host_.waitForFinished(4000) || host_.exitStatus() != QProcess::NormalExit || host_.exitCode() != 0) {
        error = QStringLiteral("Модуль панели ещё не подтвердил восстановление."); return false;
    }
    return true;
}
bool TaskbarModuleSession::configureComponents(bool taskViewLeft,int gap,unsigned trayOrder,QString& error) {
    if(gap<0 || gap>96 || trayOrder>=24 || !layoutAvailable()){error=QStringLiteral("Нативное размещение компонентов несовместимо с этой сборкой Windows.");return false;}
    if(host_.state()==QProcess::NotRunning) {
        host_.setProgram(hostPath());host_.setArguments({"--configure-taskview"});
        host_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=CREATE_NO_WINDOW;});
        host_.start();
        if(!host_.waitForStarted(3000) || !waitForLine("layout-ready")) {
            QString ignored;stop(ignored);error=QStringLiteral("Нужна совместимая исходная основная панель без другой native-сессии.");return false;
        }
    }
    const QList<QByteArray> commands{
        taskViewLeft ? QByteArray("after-widgets ")+QByteArray::number(gap)+'\n' : QByteArray("taskview-reset\n"),
        QByteArray("tray-order ")+QByteArray::number(trayOrder)+'\n'};
    for(const auto& command:commands) {
        if(host_.write(command)!=command.size() || !host_.waitForBytesWritten(3000) || !waitForLine("taskview-ok")) {
            QString rollback;
            const bool restored=stop(rollback);
            error=restored ? QStringLiteral("Windows отказала в перестановке; исходное состояние восстановлено.")
                : QStringLiteral("Перестановка и откат не подтверждены. Автоприменение новой конфигурации не сохранено.");
            return false;
        }
    }
    applied_ = true;
    return true;
}
bool TaskbarModuleSession::reorderTray(const QString& device,unsigned order,QString& error) {
    if(order>=24){error=QStringLiteral("Недопустимый порядок трея.");return false;}
    if(!device.startsWith(QStringLiteral("\\\\.\\DISPLAY")) || device.size()>32 || device.contains(' ') || device.contains('\n')){error=QStringLiteral("Недопустимый монитор.");return false;}
    if(!running()){if(!order)return true;error=QStringLiteral("Сессия оформления не запущена.");return false;}
    const auto command=QByteArray("tray-order ")+QByteArray::number(order)+' '+device.toLatin1()+'\n';
    if(host_.write(command)!=command.size() || !host_.waitForBytesWritten(3000) || !waitForLine("tray-ok "+device.toLatin1())) {
        error=QStringLiteral("Windows не подтвердила порядок трея. Оформление не отключалось.");return false;
    }
    return true;
}
}
