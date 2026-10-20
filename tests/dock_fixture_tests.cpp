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
#include <cstdlib>

void require(bool value, const char* message) { if (!value) { QTextStream(stderr) << message << '\n'; std::exit(1); } }
struct DockFixture {
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
        auto two=one; two.handle=124; two.title="Second window"; two.active=false;
        auto transient=one; transient.app.id="fixture:transient"; transient.app.executable=directory+"/third.exe"; transient.app.title="Transient"; transient.handle=125; transient.active=false;
        model.reconcile({one,two,transient});
        require(model.rowCount()==3,"Transient application absent");
        require(model.entries_[1].windows.size()==2,"Windows not grouped with pinned app");
        require(model.data(model.index(1),DockController::Active).toBool(),"Active group lost");
        model.reconcile({two});
        require(model.rowCount()==2 && !model.data(model.index(1),DockController::Active).toBool(),"Closed app/foreground reconciliation");
        model.configure("iconSize",9000); require(model.config()["iconSize"].toInt()==48,"Invalid geometry accepted");
        model.configure("iconSize",64); model.configureScreen("fixture-monitor","edge","left");
        { DockController restored; require(restored.rowCount()==0,"Read-only construction populated live model");
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
            }
        }
        if (!warnings.isEmpty()) { QTextStream(stderr)<<warnings.join('\n')<<'\n'; require(false,"QML runtime warnings"); }
        // Corruption is bounded and ignored rather than overwriting native settings.
        QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        saved.setValue("dock/configV1",QByteArray(40000,'x')); saved.sync();
        DockController recovered; require(recovered.config()["iconSize"].toInt()==48,"Oversize config recovery");
    }
};
int main(int argc,char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    QGuiApplication app(argc,argv);
    QTemporaryDir directory; require(directory.isValid(),"Temporary profile");
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    DockFixture::run(directory.path());
    QTextStream(stdout)<<"Dock isolated model, persistence and QML fixture passed\n";
}
