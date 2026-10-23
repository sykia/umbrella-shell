#include "Version.h"
#include "TaskbarSettings.h"
#include "DockController.h"
#include "SettingsWindowLifetime.h"
#include "platform/windows/SettingsWindowControl.h"
#include "platform/windows/TaskbarRuntime.h"
#include "platform/windows/SettingsPageObserver.h"
#include <QQmlContext>
#include "platform/windows/WindowBackdrop.h"
#include <QQuickItem>
#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQuickView>
#include <QSGRendererInterface>
#include <QTextStream>
#include <QTimer>
#include <QDir>
#include <QSettings>
#include <QFile>

int main(int argc, char* argv[]) {
    const bool smoke = argc == 2 && QByteArray(argv[1]) == "--ui-smoke";
    const bool settings = argc == 2 && QByteArray(argv[1]) == "--settings";
    const bool resume = argc == 2 && QByteArray(argv[1]) == "--resume-settings";
    if (argc > 1 && !smoke && !settings && !resume) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("UmbrellaShell");
        QCoreApplication::setApplicationName("UmbrellaShell");
        const auto args = app.arguments();
        if (args.size() == 2 && args[1] == "--settings-blur-observer") return umbrella::platform::runSettingsPageObserver();
        if (args.size() == 2 && args[1] == "--settings-blur-inspect") return umbrella::platform::inspectSettingsPage();
        if (args.size() == 5 && args[1] == "--taskbar-watch") {
            bool ok = false;
            const auto parent = args[2].toULong(&ok);
            return ok && parent ? umbrella::platform::watchTaskbarAppearance(parent, args[3], args[4]) : 2;
        }
        if (args.size() != 2) return 2;
        if (args[1] == "--restore-taskbar-style")
            return umbrella::platform::recoverTaskbarAppearance() ? 0 : 1;
        if (args[1] == "--diagnostics") {
            QTextStream(stdout) << QJsonDocument(QJsonObject{{"version", UMBRELLA_VERSION},
                {"qtVersion", qVersion()}, {"updatesEnabled", false},
                {"shellBarAvailable", false}}).toJson();
            return 0;
        }
        if (args[1] == "--startup-status") {
            QFile report(umbrella::platform::TaskbarRuntime::reportPath());
            if (!report.open(QIODevice::ReadOnly) || report.size() > 65536) return 1;
            QTextStream(stdout) << report.readAll(); return 0;
        }
        if (args[1] == "--help" || args[1] == "--version") {
            QTextStream(stdout) << "Umbrella Shell " << UMBRELLA_VERSION
                << "\n--settings --resume-settings --startup-status --diagnostics --ui-smoke --restore-taskbar-style\n";
            return 0;
        }
        return 2;
    }
    if (!smoke && umbrella::platform::activateExistingSettingsWindow(!resume)) return 0;
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("UmbrellaShell");
    app.setApplicationName("UmbrellaShell");
    app.setApplicationVersion(UMBRELLA_VERSION);
    umbrella::platform::TaskbarRuntime runtime;
    if (!smoke && !runtime.ownsInstance()) {
        umbrella::platform::activateExistingSettingsWindow(!resume);
        return 0;
    }
    if (resume) {
        QSettings saved(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
        if (!saved.value("resumeAppearance", saved.contains("edge")).toBool() && !saved.value("dock/enabled", false).toBool()) return 0;
    }
    app.setWindowIcon(QIcon(":/assets/umbrella.png"));
    TaskbarSettings taskbarSettings;
    DockController dock;
    umbrella::platform::SettingsBlurIntegration settingsBlur;
    QQuickView view;
    const auto updateSettingsBlur = [&] {
        settingsBlur.update(taskbarSettings.blockSystemTaskbarSettings(), taskbarSettings.managesTaskbar() || dock.enabled());
    };
    if (!smoke) {
        QObject::connect(&taskbarSettings, &TaskbarSettings::changed, &view, updateSettingsBlur);
        QObject::connect(&dock, &DockController::enabledChanged, &view, updateSettingsBlur);
    }
    dock.attachEngine(*view.engine());
    SettingsWindowLifetime settingsLifetime(app, view, !smoke);
    view.setPersistentGraphics(false);
    view.setPersistentSceneGraph(false);
    QObject::connect(&view, &QWindow::visibleChanged, &view, [&view](bool visible) {
        if (!visible) view.releaseResources();
    });
    view.rootContext()->setContextProperty("dockSystem", &dock);
    view.rootContext()->setContextProperty("taskbarSystem", &taskbarSettings);
    view.setFlags(Qt::Window | Qt::FramelessWindowHint);
    auto format = view.format();
    format.setAlphaBufferSize(8);
    view.setFormat(format);
    view.setColor(Qt::transparent);
    bool warning = false, graphicsError = false;
    int frames = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&] { warning = true; });
    QObject::connect(&view, &QQuickWindow::sceneGraphError, &app, [&] { graphicsError = true; });
    QObject::connect(&view, &QQuickWindow::frameSwapped, &app, [&] {
        ++frames;
        if (frames == 1 && view.rootObject())
            view.rootObject()->setProperty("firstFrameReady", true);
    }, Qt::QueuedConnection);
    view.setTitle(QStringLiteral("Umbrella — Настройки"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qt/qml/Umbrella/Settings.qml"));
    if (view.status() != QQuickView::Ready || QImage(":/assets/umbrella.png").isNull()) return 1;
    view.resize(1000, 700);
    view.setMinimumSize(QSize(800, 560));
    view.create();
    umbrella::platform::SettingsWindowControl settingsControl(app, view);
    const bool backdrop = umbrella::platform::applyWindowBackdrop(view);
    view.rootObject()->setProperty("backdropAvailable", backdrop);
    if (!backdrop) QTextStream(stderr) << "System blur unavailable; using opaque fallback.\n";
    QObject::connect(&dock, &DockController::settingsRequested, &view, [&view](bool dockPage) {
        if (dockPage && view.rootObject()) { view.rootObject()->setProperty("homeOpen", false); view.rootObject()->setProperty("taskbarExpanded", true); view.rootObject()->setProperty("taskbarSection", "dock"); }
        view.show(); view.raise(); view.requestActivate();
    });
    QObject::connect(&dock, &DockController::quitRequested, &app, [&app] { app.exit(0); });
    if (!smoke) dock.restore();
    QObject::connect(&taskbarSettings, &TaskbarSettings::changed, &dock, [&dock] {
        if (dock.enabled()) umbrella::platform::TaskbarRuntime::setLoginResume(true);
    });
    if (!resume) view.show();
    bool restorationAttempted = false, restorationSucceeded = false;
    if (!smoke) QObject::connect(&taskbarSettings, &TaskbarSettings::changed, &app, [&] {
        if (restorationAttempted && !taskbarSettings.pending())
            umbrella::platform::TaskbarRuntime::writeRestoreReport(taskbarSettings.savedAppearanceRestored() || dock.enabled(),
                dock.enabled() ? dock.status() : taskbarSettings.status());
    });
    if (!smoke) umbrella::platform::TaskbarRuntime::writeRestoreReport(false, QStringLiteral("Ожидание Explorer, панелей и мониторов."));
    if (!smoke && !runtime.start([&] {
        restorationAttempted = true;
        const bool handled = taskbarSettings.restoreSavedAppearance();
        restorationSucceeded = taskbarSettings.savedAppearanceRestored() || dock.enabled();
        umbrella::platform::TaskbarRuntime::writeRestoreReport(restorationSucceeded, dock.enabled() ? dock.status() : taskbarSettings.status());
        return handled;
    })) {
        QTextStream(stderr) << "Explorer lifecycle hooks unavailable.\n";
        return 1;
    }
    if (smoke) {
        QTimer::singleShot(4200, &app, [&] {
            bool saved = true;
            const QString directory = qEnvironmentVariable("UMBRELLA_SMOKE_DIR");
            if (!directory.isEmpty())
                saved = QDir().mkpath(directory) && view.grabWindow().save(directory + "/settings-0.png");
            const bool d3d = view.rendererInterface()->graphicsApi() == QSGRendererInterface::Direct3D11;
            QTextStream(stdout) << "UI smoke: frames=" << frames << " blur=" << backdrop << " d3d11=" << d3d << '\n';
            app.exit(!warning && !graphicsError && frames > 0 && view.rootObject()->property("phase").toInt() == 3
                && view.rootObject()->property("reveal").toReal() >= 1.0 && d3d && saved ? 0 : 1);
        });
    }
    return app.exec();
}
