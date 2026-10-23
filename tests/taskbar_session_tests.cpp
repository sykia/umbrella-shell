#include "platform/windows/TaskbarModuleSession.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstdio>
#include <iostream>

namespace umbrella::platform {
struct TaskbarModuleSessionTest {
    static bool lossGeneration() {
        TaskbarModuleSession session;int losses=0;
        const QString device=QStringLiteral("\\\\.\\DISPLAY1");
        session.componentLost=[&](const QString& value){if(value==device)++losses;};
        session.placements_.insert(device,1);session.deferLoss(device,1);
        session.placements_.insert(device,2);QCoreApplication::processEvents();
        if(losses)return false;
        session.deferLoss(device,2);++session.processEpoch_;QCoreApplication::processEvents();
        if(losses)return false;
        session.deferLoss(device,2);QCoreApplication::processEvents();
        return losses==1 && !session.placements_.contains(device);
    }
    static bool response(const QString& mode, bool expected) {
        TaskbarModuleSession session;
        session.host_.setProgram(QCoreApplication::applicationFilePath());
        session.host_.setArguments({mode});
        session.host_.start();
        if (!session.host_.waitForStarted(2000)) return false;
        QElapsedTimer elapsed; elapsed.start();
        const bool actual = session.waitForLine("taskview-ok");
        const bool timely = elapsed.elapsed() < 1500;
        session.host_.closeWriteChannel();
        const bool ended = session.host_.waitForFinished(2000) && session.host_.exitCode() == 0;
        return actual == expected && timely && ended;
    }
};
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc == 2) {
        const auto mode = app.arguments()[1];
        if (mode == "--ack") std::puts("diagnostic line\ntaskview-ok");
        else if (mode == "--refuse") std::puts("taskview-refused");
        else if (mode == "--error") std::puts("error incompatible-state");
        else return 2;
        std::fflush(stdout);
        // Keep stdout open until the client closes stdin. Refusal must complete
        // before EOF, rather than after the three-second response timeout.
        std::getchar();
        return 0;
    }
    using umbrella::platform::TaskbarModuleSessionTest;
    if (!TaskbarModuleSessionTest::lossGeneration() || !TaskbarModuleSessionTest::response("--ack", true) ||
        !TaskbarModuleSessionTest::response("--refuse", false) ||
        !TaskbarModuleSessionTest::response("--error", false)) {
        std::cerr << "Host acknowledgement/refusal framing failed or timed out.\n"; return 1;
    }
    std::cout << "Passed: exact acknowledgement and immediate refusal/error; mock child only, no Explorer.\n";
}
