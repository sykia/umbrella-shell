#include "platform/windows/TaskbarModuleSession.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstdio>
#include <iostream>

namespace umbrella::platform {
struct TaskbarModuleSessionTest {
    static bool hiddenDiscovery() {
        const QString first=QStringLiteral("panel-one"),second=QStringLiteral("panel-two");
        const QByteArray output="components-monitor panel-one\n  search type=Original bounds=(10,0,40,48) visible=1 hit=0\n"
            "  start type=Original bounds=(50,0,40,48) visible=1 hit=1\n"
            "components-monitor panel-two\n  search type=Original bounds=(10,0,40,48) visible=1 hit=0\n";
        const auto ordinary=TaskbarModuleSession::decodeComponents(output,{});
        const auto retained=TaskbarModuleSession::decodeComponents(output,{{first,{"search","widgets"}}});
        return ordinary[first]==QStringList{"start"} && retained[first].contains("search") &&
            !retained[first].contains("widgets") && !retained[second].contains("search");
    }

    static bool placementReport(const QString& mode,bool preserved) {
        TaskbarModuleSession session;
        session.host_.setProgram(QCoreApplication::applicationFilePath());session.host_.setArguments({mode});session.host_.start();
        if(!session.host_.waitForStarted(2000))return false;
        const bool refused=!session.waitForLine("slots-ok \\\\.\\DISPLAY1");
        const bool decoded=session.previousPlacementPreserved()==preserved && (mode!="--slots-geometry" || !session.placementFailure_.isEmpty());
        session.host_.closeWriteChannel();
        return refused && decoded && session.host_.waitForFinished(2000) && session.host_.exitCode()==0;
    }
    static bool monitorRefusal() {
        TaskbarModuleSession session;QString error;
        session.host_.setProgram(QCoreApplication::applicationFilePath());session.host_.setArguments({"--style-refuse"});session.host_.start();
        if(!session.host_.waitForStarted(2000))return false;
        const bool refused=!session.apply(80,"bottom",64,false,true,error,"\\\\.\\DISPLAY1");
        const bool survived=session.running();
        const bool rollback=session.apply(100,"bottom",48,false,false,error,"\\\\.\\DISPLAY1");
        return refused && survived && rollback && session.stop(error);
    }
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
        if(mode=="--style-refuse") {
            char line[256]{};
            while(std::fgets(line,sizeof(line),stdin)) {
                if(line[0]=='8')std::puts("error geometry-refused");else std::printf("applied %s",line);
                std::fflush(stdout);
            }
            return 0;
        }
        if (mode == "--ack") std::puts("diagnostic line\ntaskview-ok");
        else if (mode == "--refuse") std::puts("taskview-refused");
        else if (mode == "--error") std::puts("error incompatible-state");
        else if(mode=="--slots-restored")std::puts("slots-report \\\\.\\DISPLAY1 status=80070032 bound=1 items=4 passes=1 phase=1 item=5\nslots-refused \\\\.\\DISPLAY1");
        else if(mode=="--slots-uncertain")std::puts("slots-report \\\\.\\DISPLAY1 status=00000000 bound=1 items=4 passes=1 phase=0 item=0\nslots-refused \\\\.\\DISPLAY1");
        else if(mode=="--slots-geometry")std::puts("slots-report \\\\.\\DISPLAY1 status=80070032 bound=0 items=0 passes=0 phase=8 item=0\nslots-refused \\\\.\\DISPLAY1");
        else return 2;
        std::fflush(stdout);
        // Keep stdout open until the client closes stdin. Refusal must complete
        // before EOF, rather than after the three-second response timeout.
        std::getchar();
        return 0;
    }
    using umbrella::platform::TaskbarModuleSessionTest;
    if (!TaskbarModuleSessionTest::hiddenDiscovery() || !TaskbarModuleSessionTest::monitorRefusal() || !TaskbarModuleSessionTest::placementReport("--slots-restored",true) ||
        !TaskbarModuleSessionTest::placementReport("--slots-uncertain",false) || !TaskbarModuleSessionTest::placementReport("--slots-geometry",false) ||
        !TaskbarModuleSessionTest::lossGeneration() || !TaskbarModuleSessionTest::response("--ack", true) ||
        !TaskbarModuleSessionTest::response("--refuse", false) ||
        !TaskbarModuleSessionTest::response("--error", false)) {
        std::cerr << "Host acknowledgement/refusal framing failed or timed out.\n"; return 1;
    }
    std::cout << "Passed: exact acknowledgement and immediate refusal/error; mock child only, no Explorer.\n";
}
