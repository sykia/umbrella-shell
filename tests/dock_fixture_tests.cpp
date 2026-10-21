#include "app/DockController.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QTextStream>
#include <QDir>
#include <QEventLoop>
#include <cstdlib>
#include <QElapsedTimer>
#include <Windows.h>
#include <psapi.h>

void require(bool value, const char* message) { if (!value) { QTextStream(stderr) << message << '\n'; std::exit(1); } }
struct DockFixture {
    static void benchmark() {
        DockController model;model.pins_.clear();model.reconcile({});
        QList<umbrella::platform::DockApp> apps;
        for(int i=0;i<64;++i) {
            umbrella::platform::DockApp app;app.id=QString("benchmark:%1").arg(i);app.aumid=app.id;app.title="Synthetic application";
            app.icon=QImage(64,64,QImage::Format_ARGB32_Premultiplied);app.icon.fill(Qt::blue);apps.append(app);
        }
        QList<umbrella::platform::DockWindow> windows;
        for(int i=0;i<512;++i) {
            umbrella::platform::DockWindow window;window.handle=1000+i;window.app=apps[i%64];window.title=QString("Synthetic window %1").arg(i);windows.append(window);
        }
        model.reconcile(windows);
        QElapsedTimer timer;timer.start();
        for(int i=0;i<100;++i) {
            for(int n=0;n<windows.size();++n) windows[n].active=n==i%512;
            model.reconcile(windows);
        }
        const double mean=timer.nsecsElapsed()/1000000.0/100;
        auto cpu=[] {
            FILETIME creation{},exit{},kernel{},user{};require(GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user),"Process CPU query");
            return ((static_cast<quint64>(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+((static_cast<quint64>(user.dwHighDateTime)<<32)|user.dwLowDateTime);
        };
        const auto initialCpu=cpu();timer.restart();
        QEventLoop idle;QTimer::singleShot(1000,&idle,&QEventLoop::quit);idle.exec();
        const auto idleWall=timer.elapsed();const double idleCpu=(cpu()-initialCpu)/10000.0;
        PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
        require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)),"Process memory query");
        QTextStream(stdout)<<"BENCHMARK synthetic model only; no WinEvent/DWM/D3D11: windows=512 groups="<<model.rowCount()
            <<" meanReconcileMs="<<mean<<" idleWallMs="<<idleWall<<" idleCpuMs="<<idleCpu
            <<" workingSetMiB="<<memory.WorkingSetSize/1048576.0<<" privateMiB="<<memory.PrivateUsage/1048576.0<<'\n';
    }
    static void run(const QString& directory) {
        DockController model;
        require(!model.enabled(),"Constructing a model must not activate desktop integration");
        for (const auto& name : {"first.exe","second.exe"}) {
            QFile file(directory + '/' + name); require(file.open(QIODevice::WriteOnly),"Fixture file creation"); file.write("fixture");
        }
        require(model.addApplication(directory+"/first.exe"),"Add application");
        require(model.addApplication(directory+"/first.exe"),"Duplicate add is idempotent");
        require(model.rowCount()==1,"Duplicate pin");
        require(!model.addApplication(directory+"/absent.exe"),"Missing application accepted");
        require(model.addApplication(directory+"/second.exe"),"Add second application");
        const auto first=model.entries_[0].app, second=model.entries_[1].app;
        model.movePin(1,0); require(model.entries_[0].app.id==second.id,"Pin reorder");
        model.movePin(-1,100); require(model.rowCount()==2,"Invalid reorder altered model");
        umbrella::platform::DockWindow one; one.app=first; one.handle=123; one.title="First window"; one.active=true;
        one.app.id="app:fixture.desktop"; one.app.aumid="Fixture.Desktop";
        auto two=one; two.handle=124; two.title="Second window"; two.active=false;
        auto transient=one; transient.app.id="fixture:transient"; transient.app.aumid.clear(); transient.app.executable=directory+"/third.exe"; transient.app.title="Transient"; transient.handle=125; transient.active=false;
        model.reconcile({one,two,transient});
        require(model.rowCount()==3,"Transient application absent");
        require(model.entries_[1].windows.size()==2,"Windows not grouped with pinned app");
        require(model.data(model.index(1),DockController::Active).toBool(),"Active group lost");
        int notifications=0;
        const auto observed=QObject::connect(&model,&QAbstractItemModel::dataChanged,&model,[&] { ++notifications; });
        auto moved=one;moved.rect=QRect(100,100,800,600);
        model.reconcile({moved,two,transient});
        require(notifications==0,"Geometry-only snapshot unnecessarily invalidated app delegates");
        auto renamed=one;renamed.title="Renamed window";
        model.reconcile({renamed,two,transient});
        require(notifications==1,"Title change did not target only its application group");
        QObject::disconnect(observed);
        auto three=two;three.handle=126;three.title="Third window";
        model.reconcile({one,two,three});
        model.reconcile({three,one,two});
        const auto ordered=model.entries_[1].windows;
        require(ordered.size()==3 && ordered[0].handle==123 && ordered[1].handle==124 && ordered[2].handle==126,"Z-order changed the application's cycling order");
        model.configure("previews",true);
        model.requestPreview(first.id,"fixture-monitor",0,0,48,48);
        require(!model.previewShowTimer_.isActive() && !model.preview_.visible(),"Disabled Dock created desktop preview resources");
        model.configure("previews",false);
        model.reconcile({two});
        require(model.rowCount()==2 && !model.data(model.index(1),DockController::Active).toBool(),"Closed app/foreground reconciliation");
        model.configure("iconSize",9000); require(model.config()["iconSize"].toInt()==48,"Invalid geometry accepted");
        model.configure("iconSize",64); model.configureScreen("fixture-monitor","edge","left");
        require(model.persistenceTimer_.isActive(),"Settings changes must coalesce disk writes"); model.save();
        model.sortPins(); require(model.pins_[0].id==first.id,"Alphabetical pin sort");
        model.movePin(0,1);
        { DockController restored; require(restored.rowCount()==2 && !restored.enabled(),"Saved pins should load without enabling desktop integration");
          require(restored.pins_.size()==2 && restored.pins_[0].id==second.id,"Pin order persistence");
          require(restored.config()["iconSize"].toInt()==64 && restored.screenConfig("fixture-monitor")["edge"]=="left","Settings persistence"); }
        model.pin(1); model.reconcile({}); require(model.rowCount()==1,"Unpin closed application");
        QQmlEngine engine; model.attachEngine(engine);
        engine.rootContext()->setContextProperty("dockSystem",&model);
        engine.rootContext()->setContextProperty("dockScreen","fixture-monitor");
        QStringList warnings;
        QObject::connect(&engine,&QQmlEngine::warnings,&engine,[&](const QList<QQmlError>& errors) { for (const auto& e:errors) warnings.append(e.toString()); });
        for (const auto& name : {"Dock.qml","DockPage.qml"}) {
            QQmlComponent component(&engine,QUrl(QString("qrc:/qt/qml/Umbrella/")+name));
            if (component.isError()) for (const auto& e:component.errors()) QTextStream(stderr)<<e.toString()<<'\n';
            std::unique_ptr<QObject> root(component.create()); require(root!=nullptr,"Dock QML failed to instantiate");
            root->setProperty("width",640); root->setProperty("height",600);
            for (const auto& edge : {"bottom","top","left","right"}) {
                auto options=model.config(); options["edge"]=edge;
                root->setProperty("options",options); QCoreApplication::processEvents();
                if (QString(name)=="Dock.qml") require(root->property("vertical").toBool()==(QString(edge)=="left" || QString(edge)=="right"),"Vertical layout state");
            }
            model.theme_["contrast"]=true; model.theme_["animations"]=false; model.theme_["transparency"]=false; emit model.themeChanged();
            if (QString(name)=="Dock.qml") require(root->property("contrast").toBool() && !root->property("animate").toBool(),"High contrast/reduced motion");
        }
        if (!warnings.isEmpty()) { QTextStream(stderr)<<warnings.join('\n')<<'\n'; require(false,"QML runtime warnings"); }
        const auto snapshots=qEnvironmentVariable("UMBRELLA_DOCK_FIXTURE_DIR");
        if (!snapshots.isEmpty()) {
            require(QDir().mkpath(snapshots),"Screenshot directory");
            QList<umbrella::platform::DockWindow> sample;
            for (int i=0;i<4;++i) {
                auto window=transient; window.app.id=QString("fixture:%1").arg(i); window.app.executable=QString("fixture:%1").arg(i);
                window.app.title=QStringList{"Editor","Files","Terminal","Browser"}[i]; window.handle=200+i;
                window.app.icon=QImage(64,64,QImage::Format_ARGB32_Premultiplied); window.app.icon.fill(QColor::fromHsv(i*70,130,220));
                window.active=i==0; sample.append(window);
            }
            model.reconcile(sample); model.theme_["contrast"]=false; model.theme_["animations"]=true;
            for (const bool vertical : {false,true}) {
                QQuickView view; model.attachEngine(*view.engine());
                view.rootContext()->setContextProperty("dockSystem",&model); view.rootContext()->setContextProperty("dockScreen","fixture-monitor");
                view.setColor(Qt::transparent); view.setResizeMode(QQuickView::SizeRootObjectToView);
                view.setSource(QUrl("qrc:/qt/qml/Umbrella/Dock.qml")); require(view.status()==QQuickView::Ready,"Screenshot QML");
                auto options=model.config(); options["edge"]=vertical ? "left" : "bottom"; options["theme"]=vertical ? "dark" : "light";
                view.rootObject()->setProperty("options",options); view.resize(vertical?108:520,vertical?520:108); view.show();
                QEventLoop wait; QTimer::singleShot(250,&wait,&QEventLoop::quit); wait.exec();
                require(view.grabWindow().save(snapshots+(vertical?"/dock-vertical-dark.png":"/dock-horizontal-light.png")),"Offscreen rendering");
            }
        }
        // Corruption is bounded and ignored rather than overwriting native settings.
        QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        saved.setValue("dock/configV1",QByteArray(40000,'x')); saved.sync();
        DockController recovered; require(recovered.config()["iconSize"].toInt()==48,"Oversize config recovery");
        require(QFile::remove(directory+"/second.exe"),"Remove temporary executable");
        DockController unavailable; require(unavailable.pins_.size()==1 && unavailable.pins_[0].id==second.id,"Unavailable application pin was erased");
    }
};
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_BACKEND","software"); qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    QGuiApplication app(argc,argv);
    QTemporaryDir directory; require(directory.isValid(),"Temporary profile");
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    if(app.arguments().contains("--benchmark")) { DockFixture::benchmark();return 0; }
    DockFixture::run(directory.path());
    for(int i=0;i<3;++i) {
        DockController recreated;
        require(!recreated.enabled() && !recreated.theme().isEmpty(),"Repeated model/apartment lifecycle");
    }
    QTextStream(stdout)<<"Dock isolated model, persistence and QML fixture passed\n";
}
