#include "Version.h"
#include "TaskbarSettings.h"
#include "platform/windows/SystemTheme.h"
#include "WidgetBackdropProvider.h"
#include "WidgetsModel.h"
#include "ClockModel.h"
#include "SettingsWindowLifetime.h"
#include "platform/windows/SettingsWindowControl.h"
#include "platform/windows/TaskbarRuntime.h"
#include <QQmlContext>
#include "platform/windows/WindowBackdrop.h"
#include <QQuickItem>
#include <QSurfaceFormat>
#include <QScreen>
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
#include <QTemporaryDir>
#include <optional>
#include <limits>

int main(int argc, char* argv[]) {
    const bool smoke = argc == 2 && QByteArray(argv[1]) == "--ui-smoke";
    const bool settings = argc == 2 && QByteArray(argv[1]) == "--settings";
    const bool resume = argc == 2 && QByteArray(argv[1]) == "--resume-settings";
    if (argc > 1 && !smoke && !settings && !resume) {
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
    QQuickWindow::setDefaultAlphaBuffer(true);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("UmbrellaShell");
    app.setApplicationName("UmbrellaShell");
    app.setApplicationVersion(UMBRELLA_VERSION);
    std::optional<QTemporaryDir> smokePreferences;
    if(smoke) {
        smokePreferences.emplace();
        if(!smokePreferences->isValid())return 1;
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,smokePreferences->path());
    }
    umbrella::platform::TaskbarRuntime runtime;
    if (!smoke && !runtime.ownsInstance()) {
        umbrella::platform::activateExistingSettingsWindow(!resume);
        return 0;
    }
    if (resume) {
        QSettings saved(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
        if (!saved.value("resumeAppearance", saved.contains("edge")).toBool() && !saved.value("resumeComponents",false).toBool()) return 0;
    }
    app.setWindowIcon(QIcon(":/assets/umbrella.png"));
    TaskbarSettings taskbarSettings;
    umbrella::platform::SystemTheme systemTheme;
    WidgetsModel widgets;
    ClockModel clock;
    const auto updateClockUsage = [&] { clock.setActive(!widgets.instances().isEmpty()); };
    QObject::connect(&widgets, &WidgetsModel::instancesChanged, &clock, updateClockUsage);
    updateClockUsage();
    QQuickView view;
    view.engine()->addImageProvider("widgetBackdrop", new WidgetBackdropProvider);
    SettingsWindowLifetime settingsLifetime(app, view, !smoke);
    view.setPersistentGraphics(false);
    view.setPersistentSceneGraph(false);
    QObject::connect(&view, &QWindow::visibleChanged, &view, [&view](bool visible) {
        if (!visible) view.releaseResources();
    });
    view.rootContext()->setContextProperty("widgetSystem", &widgets);
    view.rootContext()->setContextProperty("clockSystem", &clock);
    QObject::connect(&view, &QWindow::visibleChanged, &widgets, [&widgets](bool visible) { if (!visible) widgets.closeEditor(); });
    view.rootContext()->setContextProperty("systemTheme", &systemTheme);
    view.rootContext()->setContextProperty("taskbarSystem", &taskbarSettings);
    view.setFlags(Qt::Window | Qt::FramelessWindowHint);
    auto format = view.format();
    format.setAlphaBufferSize(8);
    view.setFormat(format);
    view.setColor(Qt::transparent);
    bool warning = false, graphicsError = false;
    int frames = 0;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&](const QList<QQmlError>& errors) {
        warning = true;
        if (smoke) for (const auto& error : errors) QTextStream(stderr) << error.toString() << '\n';
    });
    QObject::connect(&view, &QQuickWindow::sceneGraphError, &app, [&] { graphicsError = true; });
    QObject::connect(&view, &QQuickWindow::frameSwapped, &app, [&] {
        ++frames;
        if (frames == 1 && view.rootObject())
            view.rootObject()->setProperty("firstFrameReady", true);
    }, Qt::QueuedConnection);
    view.setTitle(QStringLiteral("Umbrella — Настройки"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl("qrc:/qt/qml/Umbrella/Settings.qml"));
    if (view.status() != QQuickView::Ready || QImage(":/assets/umbrella.png").isNull()) {
        for (const auto& error : view.errors()) QTextStream(stderr) << error.toString() << '\n';
        return 1;
    }
    view.resize(1000, 700);
    view.setMinimumSize(QSize(800, 560));
    view.create();
    umbrella::platform::SettingsWindowControl settingsControl(app, view);
    bool backdrop = false;
    const auto refreshBackdrop = [&view, &systemTheme, &backdrop] {
        const auto theme = systemTheme.theme();
        backdrop = umbrella::platform::applyWindowBackdrop(view,
            theme.value("transparency").toBool() && !theme.value("contrast").toBool(),
            theme.value("dark").toBool());
        view.rootObject()->setProperty("backdropAvailable", backdrop);
    };
    refreshBackdrop();
    QObject::connect(&systemTheme, &umbrella::platform::SystemTheme::themeChanged, &view, refreshBackdrop);
    if (!backdrop) QTextStream(stderr) << "System blur unavailable; using opaque fallback.\n";
    if (!resume) view.show();
    bool restorationAttempted = false, restorationSucceeded = false;
    if (!smoke) QObject::connect(&taskbarSettings, &TaskbarSettings::changed, &app, [&] {
        if (restorationAttempted && !taskbarSettings.pending())
            umbrella::platform::TaskbarRuntime::writeRestoreReport(taskbarSettings.savedAppearanceRestored(),
                taskbarSettings.status());
    });
    if (!smoke) umbrella::platform::TaskbarRuntime::writeRestoreReport(false, QStringLiteral("Ожидание Explorer, панелей и мониторов."));
    if (!smoke && !runtime.start([&] {
        restorationAttempted = true;
        const bool handled = taskbarSettings.restoreSavedAppearance();
        restorationSucceeded = taskbarSettings.savedAppearanceRestored();
        umbrella::platform::TaskbarRuntime::writeRestoreReport(restorationSucceeded, taskbarSettings.status());
        return handled;
    })) {
        QTextStream(stderr) << "Explorer lifecycle hooks unavailable.\n";
        return 1;
    }
    bool widgetsSmoke = true;
    if (smoke) {
        QTimer::singleShot(1400, &app, [&] {
            view.rootObject()->setProperty("homeOpen", false);
            view.rootObject()->setProperty("taskbarSection", "widgets");
            widgetsSmoke = widgets.catalog().size() == 10;
            widgets.openEditor();
            widgetsSmoke = widgetsSmoke && widgets.addClock("inline") && widgets.addClock("stacked") && !widgets.addClock("invalid");
            if (widgets.instances().size() == 2) {
                widgets.selectInstance(widgets.instances().first().toMap().value("instanceId").toString());
                auto firstStyle = widgets.selectedClock().value("settings").toMap();
                firstStyle.insert("style", "blur-card");
                widgetsSmoke = widgetsSmoke && widgets.updateClock(firstStyle);
                widgets.selectInstance(widgets.instances().last().toMap().value("instanceId").toString());
                auto secondStyle = widgets.selectedClock().value("settings").toMap();
                secondStyle.insert("style", "blur-digits");
                widgetsSmoke = widgetsSmoke && widgets.updateClock(secondStyle);
            }
        });
        QTimer::singleShot(2400, &app, [&] {
            widgets.selectInstance(widgets.instances().last().toMap().value("instanceId").toString());
            widgets.select("unknown");
            widgetsSmoke = widgetsSmoke && widgets.editing() && widgets.selectedId() == "clock" && widgets.instances().size() == 2;
            const auto directory = qEnvironmentVariable("UMBRELLA_SMOKE_DIR");
            if (!directory.isEmpty()) {
                widgetsSmoke = widgetsSmoke && QDir().mkpath(directory);
                for (auto* window : QGuiApplication::allWindows()) {
                    if (window->objectName() != "widgetEditor") continue;
                    auto* quick = qobject_cast<QQuickWindow*>(window);
                    widgetsSmoke = widgetsSmoke && quick && quick->isVisible()
                        && quick->windowState() == Qt::WindowNoState
                        && quick->geometry() == quick->screen()->geometry() && QDir().mkpath(directory)
                        && quick->grabWindow().save(directory + "/widgets-editor.png");
                }
            }
        });
        QTimer::singleShot(3200, &app, [&] {
            widgets.closeEditor();
            widgetsSmoke = widgetsSmoke && !widgets.editing() && widgets.selectedId().isEmpty();
            widgets.openEditor();
            widgetsSmoke = widgetsSmoke && widgets.editing();
            widgets.closeEditor();
        });
    }
    if (smoke) {
        QTimer::singleShot(4200, &app, [&] {
            bool saved = true;
            const QString directory = qEnvironmentVariable("UMBRELLA_SMOKE_DIR");
            if (!directory.isEmpty())
                saved = QDir().mkpath(directory) && view.grabWindow().save(directory + "/settings-0.png");
            const bool d3d = view.rendererInterface()->graphicsApi() == QSGRendererInterface::Direct3D11;
            QTextStream(stdout) << "UI smoke: frames=" << frames << " blur=" << backdrop << " d3d11=" << d3d << " widgets=" << widgetsSmoke << " warnings=" << warning << " graphicsError=" << graphicsError << '\n';
            app.exit(widgetsSmoke && !warning && !graphicsError && frames > 0 && view.rootObject()->property("phase").toInt() == 3
                && view.rootObject()->property("reveal").toReal() >= 1.0 && d3d && saved ? 0 : 1);
        });
    }
    return app.exec();
}
