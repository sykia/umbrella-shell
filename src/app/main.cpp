#include "Version.h"
#include "ShellController.h"
#include "platform/windows/TaskbarVisibility.h"
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
#include <QDir>
#include <QQuickItem>
#include <functional>
int main(int argc, char* argv[]) {
    // Recovery and diagnostics must never initialize a graphics device.
    const bool smoke = argc == 2 && QByteArray(argv[1]) == "--ui-smoke";
    if (argc > 1 && !smoke) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("UmbrellaShell");
        QCoreApplication::setApplicationName("UmbrellaShell");
        const auto args = app.arguments();
        if ((args.size() == 6 || (args.size() == 7 && args[6] == "suppress")) && args[1] == "--guard") {
            bool pidOk = false, stateOk = false;
            const auto pid = args[2].toULong(&pidOk);
            const auto state = args[3].toUInt(&stateOk);
            if (!pidOk || !stateOk || !pid || state > 3) return 2;
            return umbrella::platform::guardTaskbar(pid, state, args[4].toStdWString(), args[5].toStdWString(), umbrella::platform::setAutoHide, args.size() == 7);
        }
        if (args.size() != 2) return 2;
        if (args[1] == "--taskbar-status") { QTextStream(stdout) << QString::fromStdString(umbrella::platform::taskbarStatusJson()) << "\n"; return 0; }
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
            const bool shown = umbrella::platform::restoreTaskbarWindows();
            const bool restored = umbrella::platform::setAutoHide(original) && shown;
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
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
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
            view->rootContext()->setContextProperty("panelId", "0");
            view->setResizeMode(QQuickView::SizeRootObjectToView);
            QObject::connect(view, &QQuickWindow::frameSwapped, &app, [&frames] { ++frames; }, Qt::QueuedConnection);
            QObject::connect(view, &QQuickWindow::sceneGraphError, &app, [&graphicsError] { graphicsError = true; });
        }
        bar.setSource(QUrl("qrc:/qt/qml/Umbrella/Bar.qml")); bar.resize(640, 36);
        settings.setSource(QUrl("qrc:/qt/qml/Umbrella/Settings.qml")); settings.resize(680, 440);
        if (bar.status() == QQuickView::Error || settings.status() == QQuickView::Error ||
            QImage(":/assets/umbrella.png").isNull()) return 1;
        bar.show(); settings.show();
        const auto finish = [&] {
            const auto memory = umbrella::platform::workingSetBytes();
            QTextStream(stdout) << "UI smoke: frames=" << frames << " workingSetBytes=" << memory
                << " backend=" << static_cast<int>(bar.rendererInterface()->graphicsApi()) << '\n';
            app.exit(!graphicsError && !qmlWarning && memory > 0 && frames >= 2 &&
                bar.rendererInterface()->graphicsApi() == QSGRendererInterface::Direct3D11 ? 0 : 1);
        };
        const QString previewDirectory = qEnvironmentVariable("UMBRELLA_SMOKE_DIR");
        std::function<void(int)> previewPage;
        if (!previewDirectory.isEmpty()) {
            if (!QDir().mkpath(previewDirectory)) return 1;
            previewPage = [&, previewDirectory](int page) {
                settings.rootObject()->setProperty("page", page);
                QTimer::singleShot(250, &app, [&, page, previewDirectory] {
                    const bool saved = settings.grabWindow().save(previewDirectory + "/settings-" + QString::number(page) + ".png");
                    const bool barSaved = page != 0 || bar.grabWindow().save(previewDirectory + "/bar.png");
                    if (!saved || !barSaved) { app.exit(1); return; }
                    if (page < 5) previewPage(page + 1); else finish();
                });
            };
            QTimer::singleShot(300, &app, [&] { previewPage(0); });
            QTimer::singleShot(15000, &app, [&] { app.exit(1); });
        } else QTimer::singleShot(2000, &app, finish);
        return app.exec();
    }
    if (!shell.start()) return 1;
    return app.exec();
}
