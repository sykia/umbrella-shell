#include "Version.h"
#include "platform/windows/ExplorerRecovery.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("UmbrellaShell");
    QCoreApplication::setApplicationName("UmbrellaShell");
    QCoreApplication::setApplicationVersion(UMBRELLA_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Umbrella Shell foundation: no shell UI or background services.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"diagnostics", "Print foundation diagnostics as JSON and exit."});
    parser.addOption({"restore-explorer", "Open the Windows Explorer executable and exit."});
    parser.process(app);
    if (!parser.positionalArguments().isEmpty()) {
        QTextStream(stderr) << "Unexpected positional arguments\n";
        return 2;
    }
    if (parser.isSet("diagnostics") && parser.isSet("restore-explorer")) return 2;
    if (parser.isSet("restore-explorer")) {
        const auto error = umbrella::platform::openExplorer();
        if (error) QTextStream(stderr) << QString::fromStdString(error.message()) << '\n';
        return error ? 1 : 0;
    }
    if (parser.isSet("diagnostics")) {
        const QJsonObject diagnostic{
            {"version", UMBRELLA_VERSION}, {"qtVersion", qVersion()},
            {"updatesEnabled", false}, {"shellReplacementEnabled", false},
            {"settingsDirectory", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)}
        };
        QTextStream(stdout) << QJsonDocument(diagnostic).toJson();
        return 0;
    }
    parser.showHelp(0);
}
