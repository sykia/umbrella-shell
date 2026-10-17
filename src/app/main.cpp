#include "Version.h"
#include "ShellController.h"
#include "platform/windows/ExplorerRecovery.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QSGRendererInterface>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <QImage>
#include <QIcon>
int main(int argc, char* argv[]) {
    // Recovery and diagnostics must never initialize a graphics device.
    const bool smoke = argc == 2 && QByteArray(argv[1]) == "--ui-smoke";
    if (argc > 1 && !smoke) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("UmbrellaShell");
        QCoreApplication::setApplicationName("UmbrellaShell");
        const auto args = app.arguments();
        if (args.size() == 6 && args[1] == "--guard") {
            bool pidOk = false, stateOk = false;
            const auto pid = args[2].toULong(&pidOk);
            const auto state = args[3].toUInt(&stateOk);
            if (!pidOk || !stateOk || !pid || state > 3) return 2;
            return umbrella::platform::guardTaskbar(pid, state, args[4].toStdWString(), args[5].toStdWString());
        }
        if (args.size() != 2) return 2;
        if (args[1] == "--diagnostics") {
            QTextStream(stdout) << QJsonDocument(QJsonObject{{"version", UMBRELLA_VERSION},
                {"qtVersion", qVersion()}, {"updatesEnabled", false}, {"shellBarAvailable", true},
                {"settingsDirectory", QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)}}).toJson();
            return 0;
        }
        if (args[1] == "--restore-taskbar") {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Shell");
            settings.setValue("shell/replacement", false); settings.sync();
            if (umbrella::platform::requestTaskbarRecovery()) return 0;
            const bool original = settings.value("recovery/pending", false).toBool()
                ? settings.value("recovery/autoHide", false).toBool() : false;
            const bool restored = umbrella::platform::setAutoHide(original);
            if (restored) { settings.remove("recovery"); settings.sync(); }
            return restored ? 0 : 1;
        }
        if (args[1] == "--restore-explorer") return umbrella::platform::openExplorer() ? 1 : 0;
        if (args[1] == "--help" || args[1] == "--version") {
            QTextStream(stdout) << "Umbrella Shell " << UMBRELLA_VERSION
                << "\n--diagnostics --restore-taskbar --restore-explorer\n";
            return 0;
        }
        return 2;
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("UmbrellaShell"); app.setApplicationName("UmbrellaShell");
    app.setApplicationVersion(UMBRELLA_VERSION); app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/assets/umbrella.png"));
    ShellController shell;
    if (smoke) {
        QQuickView bar, settings;
        int frames = 0;
        bool qmlWarning = false;
        bool graphicsError = false;
        for (auto* view : {&bar, &settings}) {
            QObject::connect(view->engine(), &QQmlEngine::warnings, &app, [&qmlWarning] { qmlWarning = true; });
            view->rootContext()->setContextProperty("shell", &shell);
            view->setResizeMode(QQuickView::SizeRootObjectToView);
            QObject::connect(view, &QQuickWindow::frameSwapped, &app, [&frames] { ++frames; }, Qt::QueuedConnection);
            QObject::connect(view, &QQuickWindow::sceneGraphError, &app, [&graphicsError] { graphicsError = true; });
        }
        bar.setSource(QUrl("qrc:/qt/qml/Umbrella/Bar.qml")); bar.resize(640, 56);
        settings.setSource(QUrl("qrc:/qt/qml/Umbrella/Settings.qml")); settings.resize(680, 440);
        if (bar.status() == QQuickView::Error || settings.status() == QQuickView::Error ||
            QImage(":/assets/umbrella.png").isNull()) return 1;
        bar.show(); settings.show();
        QTimer::singleShot(2000, &app, [&] {
            const auto memory = umbrella::platform::workingSetBytes();
            QTextStream(stdout) << "UI smoke: frames=" << frames << " workingSetBytes=" << memory
                << " backend=" << static_cast<int>(bar.rendererInterface()->graphicsApi()) << '\n';
            app.exit(!graphicsError && !qmlWarning && memory > 0 && frames >= 2 &&
                bar.rendererInterface()->graphicsApi() == QSGRendererInterface::Direct3D11 ? 0 : 1);
        });
        return app.exec();
    }
    if (!shell.start()) return 1;
    return app.exec();
}
