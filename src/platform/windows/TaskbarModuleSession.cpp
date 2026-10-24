#include "TaskbarModuleSession.h"
#include "explorer/NativeTaskbarLayout.h"
#include "explorer/TaskbarPlacementCommand.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <Windows.h>
#include <algorithm>

namespace umbrella::platform {
namespace {
QString hostPath() { return QCoreApplication::applicationDirPath() + "/UmbrellaTaskbarHost.exe"; }
}
bool TaskbarModuleSession::available() {
    return QFileInfo::exists(hostPath()) && QFileInfo::exists(QCoreApplication::applicationDirPath() + "/UmbrellaTaskbarModule.dll");
}
bool TaskbarModuleSession::layoutAvailable() { return available() && explorer::native_layout::installedBuildSupported(); }
QHash<QString,QStringList> TaskbarModuleSession::discoverComponents(QString& error,const QHash<QString,QStringList>& retainedHidden) {
    QHash<QString,QStringList> result;
    if(!layoutAvailable()){error=QStringLiteral("Сборка Explorer не поддерживает обнаружение компонентов.");return result;}
    QProcess probe;
    probe.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=CREATE_NO_WINDOW;});
    probe.start(hostPath(),{"--inspect-components"});
    if(!probe.waitForFinished(3000) || probe.exitStatus()!=QProcess::NormalExit || probe.exitCode()!=0){error=QStringLiteral("Не удалось получить состав панелей Explorer.");return result;}
    const auto output=probe.readAllStandardOutput();
    if(output.size()>65536){error=QStringLiteral("Слишком большой снимок компонентов.");return result;}
    return decodeComponents(output,retainedHidden);
}
QHash<QString,QStringList> TaskbarModuleSession::decodeComponents(const QByteArray& output,const QHash<QString,QStringList>& retainedHidden) {
    QHash<QString,QStringList> result;
    if(output.size()>65536)return result;
    QString device;
    QHash<QString,QHash<QString,double>> positions;
    const QRegularExpression item(QStringLiteral("^  ([a-z]+) .*bounds=\\(([-0-9.]+),([-0-9.]+),([0-9.]+),([0-9.]+)\\) visible=1 hit=([01])"));
    for(const auto& line:QString::fromUtf8(output).split('\n')) {
        if(line.startsWith("components-monitor ")){device=line.mid(19).trimmed();if(result.size()<16)result.insert(device,{});else device.clear();continue;}
        const auto match=item.match(line);
        if(device.isEmpty() || !match.hasMatch() || match.captured(4).toDouble()<=0 || match.captured(5).toDouble()<=0)continue;
        auto id=match.captured(1);
        if(match.captured(6)=="0" && !retainedHidden.value(device).contains(id))continue;
        const auto wide=id.toStdWString();
        if(taskbar::componentFromId(wide)==taskbar::Component::Unknown)continue;
        auto& ids=result[device];
        const auto remember=[&](const QString& role) {
            if(!ids.contains(role))ids.append(role);
            const auto x=match.captured(2).toDouble();
            if(!positions[device].contains(role) || x<positions[device][role])positions[device][role]=x;
        };
        if(id=="quicksettings"){remember("network");remember("volume");}else remember(id);
    }
    for(auto i=result.begin();i!=result.end();++i)std::stable_sort(i.value().begin(),i.value().end(),[&](const auto& a,const auto& b){return positions[i.key()][a]<positions[i.key()][b];});
    return result;
}
TaskbarModuleSession::TaskbarModuleSession() {
    QObject::connect(&host_,&QProcess::started,&host_,[this]{++processEpoch_;placements_.clear();});
    QObject::connect(&host_,&QProcess::readyReadStandardOutput,&host_,[this]{readOutput();});
    QObject::connect(&host_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&host_,[this](int,QProcess::ExitStatus){
        if(!stopping_)for(auto i=placements_.cbegin();i!=placements_.cend();++i)deferLoss(i.key(),i.value());
    });
}
void TaskbarModuleSession::deferLoss(const QString& device,unsigned serial) {
    const auto epoch=processEpoch_;
    QMetaObject::invokeMethod(&host_,[this,device,serial,epoch]{
        if(epoch!=processEpoch_ || placements_.value(device)!=serial)return;
        placements_.remove(device);
        if(componentLost)componentLost(device);
    },Qt::QueuedConnection);
}
void TaskbarModuleSession::readOutput() {
    while(host_.canReadLine()) {
        const auto line=host_.readLine(1024).trimmed();
        const auto fields=line.split(' ');
        if(fields.size()==3 && (fields[0]=="slots-lost" || fields[0]=="slots-applied")) {
            bool valid=false;const auto serial=fields[2].toUInt(&valid);
            if(!valid)continue;
            const auto device=QString::fromLatin1(fields[1]);
            if(fields[0]=="slots-lost"){if(serial)deferLoss(device,serial);}
            else if(serial)placements_.insert(device,serial);
            else placements_.remove(device);
            continue;
        }
        if(replies_.size()>=64)replies_.dequeue();
        replies_.enqueue(line);
    }
}
TaskbarModuleSession::~TaskbarModuleSession() { QString ignored; stop(ignored); }
bool TaskbarModuleSession::waitForLine(const QByteArray& expected) {
    QByteArray placementDiagnostic;
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 3000) {
        readOutput();
        while (!replies_.isEmpty()) {
            const auto line = replies_.dequeue();
            if(line.startsWith("surface-geometry "))qWarning().noquote()<<line;
            if(line.startsWith("surface-detail ")) {
                const auto detail=QRegularExpression(QStringLiteral("^surface-detail status=([0-9a-fA-F]{8})$")).match(QString::fromLatin1(line));
                if(detail.hasMatch())switch(detail.captured(1).toUInt(nullptr,16)&0xffffu) {
                case 0x301:placementFailure_=QStringLiteral("Ширина XAML не совпадает с новым размером панели.");break;
                case 0x302:placementFailure_=QStringLiteral("Высота XAML не совпадает с новой толщиной панели.");break;
                case 0x303:placementFailure_=QStringLiteral("Начало координат XAML не совпадает с поверхностью панели.");break;
                case 0x304:placementFailure_=QStringLiteral("Масштаб XAML не совпадает с DPI панели.");break;
                default:placementFailure_=QStringLiteral("Windows не подтвердила измеренную геометрию панели.");break;
                }
            }
            if(line.startsWith("slots-report ")) {
                placementDiagnostic=line;
                const auto rollback=QRegularExpression(QStringLiteral(" status=([0-9a-fA-F]{8}) bound=([01]) ")).match(QString::fromLatin1(line));
                if(rollback.hasMatch())placementPreserved_=rollback.captured(2)=="1" && (rollback.captured(1).toUInt(nullptr,16)&0x80000000u);
                const auto detail=QRegularExpression(QStringLiteral(" phase=([0-9]+) item=([0-9]+)$")).match(QString::fromLatin1(line));
                if(detail.hasMatch())switch(detail.captured(1).toUInt()) {
                case 1:placementFailure_=detail.captured(2).toUInt()==5?
                    QStringLiteral("Новая геометрия не вмещает раскладку без пересечений."):
                    QStringLiteral("Измеренные компоненты не позволяют построить запрошенную раскладку.");break;
                case 4:placementFailure_=QStringLiteral("Windows не подтвердила область нажатия исходного компонента.");break;
                case 6:placementFailure_=QStringLiteral("Explorer повторно меняет позиции; достигнут предел безопасных попыток.");break;
                case 7:placementFailure_=QStringLiteral("Не удалось применить или проверить общую маску панели.");break;
                case 8:placementFailure_=QStringLiteral("Размеры, начало координат или DPI XAML не совпадают с фактической горизонтальной панелью. Боковые панели и автоскрытие не поддерживаются.");break;
                default:break;
                }
            }
            if(line.startsWith("slots-refused ") && !placementDiagnostic.isEmpty())qWarning().noquote()<<placementDiagnostic;
            if (line == expected) return true;
            if (line.startsWith("error ") || line.startsWith("FAILED") || line == "taskview-refused" || line.startsWith("tray-refused ") || line.startsWith("slots-refused ")) return false;
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
    placementFailure_.clear();
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
        error = placementFailure_.isEmpty()?QStringLiteral("Модуль Explorer не подтвердил геометрию выбранной панели; требуется откат её параметров."):placementFailure_; return false;
    }
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
    return true;
}
bool TaskbarModuleSession::stop(QString& error) {
    if (host_.state() == QProcess::NotRunning) return true;
    stopping_=true;
    host_.closeWriteChannel(); // EOF restores the subclass and original rectangle, then exits.
    if (!host_.waitForFinished(4000) || host_.exitStatus() != QProcess::NormalExit || host_.exitCode() != 0) {
        stopping_=false;
        error = QStringLiteral("Модуль панели ещё не подтвердил восстановление."); return false;
    }
    placements_.clear();replies_.clear();stopping_=false;
    return true;
}
bool TaskbarModuleSession::configureComponents(bool taskViewLeft,int gap,unsigned trayOrder,QString& error) {
    if(gap<0 || gap>96 || trayOrder>=24 || !layoutAvailable()){error=QStringLiteral("Нативное размещение компонентов несовместимо с этой сборкой Windows.");return false;}
    if(running() && host_.arguments().contains("--components") && !stop(error))return false;
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
bool TaskbarModuleSession::placeComponents(const QByteArray& command,const QString& device,QString& error) {
    placementFailure_.clear();
    placementPreserved_=false;
    explorer::PlacementCommand parsed;
    if(!explorer::parsePlacementCommand(std::string_view(command.constData(),static_cast<size_t>(command.size())),parsed) ||
        parsed.statusOnly || QString::fromLatin1(parsed.device)!=device || !layoutAvailable()) {
        error=QStringLiteral("Нативное размещение компонентов несовместимо с запросом или сборкой Windows.");return false;
    }
    if(running() && host_.arguments().contains("--configure-taskview") && !stop(error))return false;
    if(!running()) {
        replies_.clear();host_.setProgram(hostPath());host_.setArguments({"--components"});
        host_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=CREATE_NO_WINDOW;});
        host_.start();
        if(!host_.waitForStarted(3000) || !waitForLine("slots-ready")) {
            QString ignored;stop(ignored);error=QStringLiteral("Не удалось подключить сессию компонентов Explorer.");return false;
        }
    }
    if(host_.write(command)!=command.size() || !host_.waitForBytesWritten(3000) || !waitForLine("slots-ok "+device.toLatin1())) {
        error=placementFailure_.isEmpty()?QStringLiteral("Windows отказала в переносе. Новый порядок не сохранён."):placementFailure_;return false;
    }
    return true;
}
}
