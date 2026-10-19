#include "app/SettingsWindowLifetime.h"
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QWindow window;
    SettingsWindowLifetime lifetime(app, window, true);
    if (app.quitOnLastWindowClosed()) return 1;
    window.show();
    QCoreApplication::processEvents();
    QCloseEvent close;
    QCoreApplication::sendEvent(&window, &close);
    if (close.isAccepted() || window.isVisible()) return 2;
    // The same window/owner remains alive; a native show command may reopen it.
    window.show();
    if (!window.isVisible()) return 3;
    window.close();
    // A normal close must not enqueue an implicit quit before this timer runs.
    bool reached = false;
    QTimer::singleShot(20, &app, [&] { reached = true; app.quit(); });
    if (app.exec() != 0 || !reached) return 4;
    // Smoke windows retain ordinary close behavior.
    QWindow smoke;
    SettingsWindowLifetime smokeLifetime(app, smoke, false);
    smoke.show();
    QCloseEvent normal;
    QCoreApplication::sendEvent(&smoke, &normal);
    if (!normal.isAccepted()) return 5;
    std::puts("Passed: close hides without quitting, same owner reopens, explicit quit works, smoke close unchanged.");
    return 0;
}
