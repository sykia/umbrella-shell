#include "app/SettingsWindowLifetime.h"
#include <QCoreApplication>
#include <QTimer>
#include <cstdio>

int main(int argc, char** argv) {
    std::fputs("lifetime: initializing Qt\n", stderr);
    QGuiApplication app(argc, argv);
    QWindow window;
    SettingsWindowLifetime lifetime(app, window, true);
    std::fputs("lifetime: showing window\n", stderr);
    if (app.quitOnLastWindowClosed()) return 1;
    window.show();
    QCoreApplication::processEvents();
    std::fputs("lifetime: delivering close\n", stderr);
    QCloseEvent close;
    QCoreApplication::sendEvent(&window, &close);
    if (close.isAccepted() || window.isVisible()) return 2;
    // The same window/owner remains alive; a native show command may reopen it.
    window.show();
    if (!window.isVisible()) return 3;
    window.close();
    std::fputs("lifetime: entering event loop\n", stderr);
    // A normal close must not enqueue an implicit quit before this timer runs.
    bool reached = false;
    // Match the native explicit-shutdown path. QGuiApplication::quit() sends
    // Close to hidden windows and is intentionally rejected by hide-on-close.
    QTimer::singleShot(20, &app, [&] { reached = true; app.exit(0); });
    if (app.exec() != 0 || !reached) return 4;
    std::fputs("lifetime: checking smoke close\n", stderr);
    // Smoke windows retain ordinary close behavior.
    QWindow smoke;
    SettingsWindowLifetime smokeLifetime(app, smoke, false);
    smoke.show();
    QCloseEvent normal;
    QCoreApplication::sendEvent(&smoke, &normal);
    if (!normal.isAccepted()) return 5;
    std::fputs("lifetime: finished\n", stderr);
    std::puts("Passed: close hides without quitting, same owner reopens, explicit quit works, smoke close unchanged.");
    return 0;
}
