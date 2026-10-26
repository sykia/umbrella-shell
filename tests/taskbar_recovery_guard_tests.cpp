// Fail-closed replay tests: deliberately never admit a native operation.
#include "app/TaskbarSettings.h"
#include "platform/windows/TaskbarRuntime.h"
#include <Windows.h>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdlib>
#include <iostream>
struct TaskbarSettingsFixture {
    static void lose(TaskbarSettings& model,const QString& device) {
        model.activeComponents_=model.confirmedComponents_;
        model.componentSession_.componentLost(device);
    }
};

void require(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    if(FindWindowW(L"Shell_TrayWnd",nullptr) && !umbrella::platform::TaskbarRuntime::monitors().isEmpty()) {
        const auto owners=umbrella::platform::TaskbarRuntime::panelOwners();
        require(!owners.isEmpty() && owners==umbrella::platform::TaskbarRuntime::panelOwners(),
            "Stable Explorer owner identity unavailable for recovery budget reset");
    }
    QTemporaryDir sandbox;
    require(sandbox.isValid(), "temporary settings unavailable");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, sandbox.path());
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "UmbrellaShell", "Taskbar");
    const QByteArray profiles = R"({"monitor-a":{"edge":"bottom","detached":true,"widthPercent":80,"thickness":64,"islands":true,"transparency":35}})";
    settings.setValue("appearanceProfilesV1", profiles);
    settings.setValue("resumeAppearance", true);
    settings.setValue("resumeComponents", false);
    for (const auto value : {QVariant(2), QVariant(3), QVariant(-1), QVariant("invalid")}) {
        settings.setValue("recovery/unsettledAttempts", value); settings.sync();
        for (int restart = 0; restart < 3; ++restart) {
            TaskbarSettings model;
            require(model.restoreSavedAppearance(), "terminal refusal requested a runtime retry");
            require(!model.savedAppearanceRestored(), "blocked replay reported native success");
            require(!model.status().isEmpty(), "blocked replay has no explanation");
        }
        settings.sync();
        require(settings.value("appearanceProfilesV1").toByteArray() == profiles, "blocked replay changed profiles");
        require(settings.value("resumeAppearance").toBool() && !settings.value("resumeComponents").toBool(),
            "blocked replay changed independent resume intent");
        require(settings.value("recovery/unsettledAttempts") == value, "blocked replay replenished budget");
    }
    settings.setValue("resumeAppearance", false);
    settings.setValue("resumeComponents", true); settings.sync();
    TaskbarSettings components;
    require(components.restoreSavedAppearance() && !components.savedAppearanceRestored(),
        "components-only replay bypassed guard");
    QString primary;
    for (const auto& entry : components.appearanceMonitors())
        if (entry.toMap()["primary"].toBool()) primary = entry.toMap()["id"].toString();
    if (!primary.isEmpty()) {
        const auto draft = QJsonDocument::fromJson(R"({"version":1,"gap":8,"available":[],"left":["taskview"],"center":[],"right":[]})").object();
        const auto confirmed = QJsonDocument(QJsonObject{{primary, draft}}).toJson(QJsonDocument::Compact);
        settings.setValue("confirmedComponentsV2", confirmed); settings.sync();
        TaskbarSettings model;
        model.apply("left", true, 35, false, 80, 64, true);
        require(!model.status().isEmpty(), "unsupported vertical geometry not refused before native reset");
        settings.sync();
        require(settings.value("confirmedComponentsV2").toByteArray() == confirmed &&
            settings.value("appearanceProfilesV1").toByteArray() == profiles,
            "refused appearance edit changed persisted layout or profiles");
    }
    const auto monitors=components.layoutMonitors();
    if(monitors.size()==2 && !primary.isEmpty()) {
        const auto draft=QJsonDocument::fromJson(R"({"version":1,"gap":8,"available":[],"left":["taskview"],"center":[],"right":[]})").object();
        QJsonObject profiles;QString primaryDevice;
        for(const auto& entry:monitors) {
            const auto monitor=entry.toMap();profiles.insert(monitor["id"].toString(),draft);
            if(monitor["id"].toString()==primary)primaryDevice=monitor["device"].toString();
        }
        const auto confirmed=QJsonDocument(profiles).toJson(QJsonDocument::Compact);
        settings.setValue("confirmedComponentsV2",confirmed);settings.setValue("resumeAppearance",false);settings.setValue("resumeComponents",true);settings.sync();
        TaskbarSettings lost;TaskbarSettingsFixture::lose(lost,primaryDevice);settings.sync();
        require(lost.activeComponentConfigurations().size()==1 && !lost.activeComponentConfigurations().contains(primary),
            "Native loss affected the other monitor");
        require(lost.confirmedComponentConfigurations().size()==2 && lost.confirmedComponentConfigurations().contains(primary),
            "Native loss hid the last confirmed layout from presentation");
        require(settings.value("confirmedComponentsV2").toByteArray()==confirmed &&
            !settings.value("resumeAppearance").toBool() && settings.value("resumeComponents").toBool(),
            "Native loss discarded last confirmed intent or changed resume flags");
    }
    settings.setValue("resumeAppearance", false);
    settings.remove("resumeComponents");
    settings.setValue("recovery/unsettledAttempts", 0); settings.sync();
    TaskbarSettings disabled;
    require(disabled.restoreSavedAppearance(), "disabled legacy replay failed");
    settings.sync();
    require(settings.value("recovery/unsettledAttempts").toInt() == 0,
        "disabled legacy replay consumed recovery budget");
    std::cout << "Recovery guard: persistent refusal, owner restarts, corrupt state and independent flags passed\n";
}
