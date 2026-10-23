// Independent QML fixture. Does not instantiate TaskbarSettings or touch Settings/Explorer.
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlEngine>
#include <QImage>
#include <QTimer>
#include <QEventLoop>
#include <QDir>
#include <cstdio>

namespace {
void wait(int milliseconds) {
    QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec();
}
}
int main(int argc, char** argv) {
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    bool warning = false;
    QObject::connect(view.engine(), &QQmlEngine::warnings, &app, [&] { warning = true; });
    QObject::connect(&view, &QQuickWindow::sceneGraphError, &app, [&] { warning = true; });
    view.setSource(QUrl("qrc:/qt/qml/Umbrella/SystemTaskbarOverlay.qml"));
    if (view.status() != QQuickView::Ready || QImage(":/assets/umbrella.png").isNull()) return 1;
    auto* root = view.rootObject();
    int frames = 0;
    QObject::connect(&view, &QQuickWindow::frameSwapped, &app, [&] { ++frames; }, Qt::QueuedConnection);
    view.resize(640, 400); view.show(); wait(350);
    const auto start = frames; wait(700);
    if (frames - start < 5 || !root->property("running").toBool()) return 2;
    root->setProperty("animationsEnabled", false); wait(200);
    const auto reducedStart = frames; wait(350);
    if (frames - reducedStart > 3 || root->property("running").toBool()) return 3;
    const auto output = qEnvironmentVariable("UMBRELLA_BLUR_FIXTURE_DIR");
    for (bool dark : {false, true}) {
        root->setProperty("darkTheme", dark);
        root->setProperty("accentColor", QColor(dark ? "#8cbfff" : "#005fb8"));
        view.setColor(dark ? QColor("#252525") : QColor("#f3f3f3"));
        view.resize(dark ? 420 : 640, 400); wait(200);
        const auto image = view.grabWindow();
        if (image.isNull()) return 4;
        if (!output.isEmpty() && (!QDir().mkpath(output) || !image.save(output + (dark ? "/dark.png" : "/light.png")))) return 5;
    }
    root->setProperty("animationsEnabled", true); view.hide(); wait(100);
    if (root->property("running").toBool()) return 6;
    const auto hidden = frames; wait(300);
    if (frames - hidden > 1 || warning) return 7;
    std::printf("Overlay QML: D3D11 frames=%d; reduced motion, hide, two themes/sizes passed. No system integration tested.\n", frames);
    return 0;
}
