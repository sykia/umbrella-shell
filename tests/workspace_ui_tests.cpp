#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
#include <iostream>

class PreferencesFixture final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap theme READ theme CONSTANT)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
public:
    QVariantMap theme() const { return {{"ink", "#111111"}, {"surface", "#eeeeee"}, {"raised", "#ffffff"}, {"accent", "#dd1234"}}; }
    QString fontFamily() const { return "Segoe UI"; }
};
class WorkspaceFixture final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* preferences READ preferences CONSTANT)
    Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY changed)
    Q_PROPERTY(QVariantMap moduleZones READ moduleZones NOTIFY changed)
    Q_PROPERTY(QString keyboardLayout READ keyboardLayout CONSTANT)
public:
    PreferencesFixture appearance;
    int active = 0, count = 5, clicks = 0;
    QObject* preferences() { return &appearance; }
    int workspaceCount() const { return count; }
    QVariantMap moduleZones() const { return {{"workspaces", 0}, {"keyboard", -1}, {"volume", -1}}; }
    QString keyboardLayout() const { return "EN"; }
    Q_INVOKABLE QVariantList workspaceModel(const QString&) const {
        QVariantList rows;
        for (int i = 0; i < count; ++i) rows << QVariantMap{{"index", i}, {"active", i == active}, {"occupied", i == 0}};
        return rows;
    }
    Q_INVOKABLE void selectWorkspace(const QString& panel, int index) {
        if (panel != "fixture" || index < 0 || index >= count) return;
        active = index; ++clicks; emit changed();
    }
signals:
    void changed();
};
int main(int argc, char** argv) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);
    if (argc != 2) return 1;
    WorkspaceFixture shell;
    QQuickView view;
    bool warnings = false;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&] { warnings = true; });
    view.rootContext()->setContextProperty("shell", &shell);
    view.rootContext()->setContextProperty("panelId", "fixture");
    view.setInitialProperties({{"zone", 0}});
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (view.status() != QQuickView::Ready) return 1;
    auto* root = view.rootObject();
    view.resize(250, 36); view.show();
    QTimer::singleShot(300, &app, [&] {
        for (int click = 0; click < 30; ++click) {
            const int index = (click + 1) % shell.count;
            auto* dot = root->findChild<QObject*>("workspaceDot" + QString::number(index));
            if (!dot || !dot->property("enabled").toBool() || !QMetaObject::invokeMethod(dot, "clicked") || shell.active != index) { app.exit(1); return; }
            // State updates must keep the delegate alive so width/color animations can finish.
            emit shell.changed();
            if (root->findChild<QObject*>(dot->objectName()) != dot || !dot->property("checked").toBool()) { app.exit(1); return; }
        }
        shell.count = 2; shell.active = 0; emit shell.changed();
        if (!root->findChild<QObject*>("workspaceDot1") || root->findChild<QObject*>("workspaceDot2")) { app.exit(1); return; }
        QTimer::singleShot(250, &app, [&] {
            std::cout << "Workspace QML: 30 routed clicks, stable delegates and count resize\n";
            app.exit(warnings || shell.clicks != 30 ? 1 : 0);
        });
    });
    QTimer::singleShot(5000, &app, [&] { app.exit(1); });
    return app.exec();
}
#include "workspace_ui_tests.moc"
