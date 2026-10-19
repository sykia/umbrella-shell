#include "Version.h"
#include "TaskbarSettings.h"
#include "SettingsWindowLifetime.h"
#include "platform/windows/SettingsWindowControl.h"
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

int main(int argc, char* argv[]) {
    const bool smoke = argc == 2 && QByteArray(argv[1]) == "--ui-smoke";
    const bool settings = argc == 2 && QByteArray(argv[1]) == "--settings";
    if (argc > 1 && !smoke && !settings) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("UmbrellaShell");
        QCoreApplication::setApplicationName("UmbrellaShell");
        const auto args = app.arguments();
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
        if (args[1] == "--help" || args[1] == "--version") {
            QTextStream(stdout) << "Umbrella Shell " << UMBRELLA_VERSION
                << "\n--settings --diagnostics --ui-smoke --restore-taskbar-style\n";
            return 0;
        }
        return 2;
    }
    if (!smoke && umbrella::platform::activateExistingSettingsWindow()) return 0;
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("UmbrellaShell");
    app.setApplicationName("UmbrellaShell");
    app.setApplicationVersion(UMBRELLA_VERSION);
    app.setWindowIcon(QIcon(":/assets/umbrella.png"));
    TaskbarSettings taskbarSettings;
    QQuickView view;
    SettingsWindowLifetime settingsLifetime(app, view, !smoke);
    view.setPersistentGraphics(false);
    view.setPersistentSceneGraph(false);
    QObject::connect(&view, &QWindow::visibleChanged, &view, [&view](bool visible) {
        if (!visible) view.releaseResources();
    });
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
    view.show();
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
