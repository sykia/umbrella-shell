// Explicit real-desktop test. Never register with unattended CTest.
#include "app/TaskbarSettings.h"
#include "platform/windows/TaskbarRuntime.h"
#include "platform/windows/TaskbarState.h"
#include "platform/windows/TaskbarWindows.h"
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QElapsedTimer>
#include <QDir>
#include <QtTest/QTest>
#include <Windows.h>
#include <vector>
#include <algorithm>

using umbrella::platform::TaskbarRuntime;
namespace {
struct Panel { QString device; RECT rect{}, monitor{}; UINT dpi{}; int alpha = -1, regionBytes = 0; };
std::vector<Panel> panels() {
    std::vector<Panel> result;
    const auto inspect = [](HWND window, LPARAM context) -> BOOL {
        wchar_t name[64]{}; GetClassNameW(window, name, 64);
        if (wcscmp(name, L"Shell_TrayWnd") && wcscmp(name, L"Shell_SecondaryTrayWnd")) return TRUE;
        MONITORINFOEXW info{}; info.cbSize = sizeof(info);
        Panel panel;
        if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONULL), &info) || !GetWindowRect(window, &panel.rect)) return TRUE;
        panel.device = QString::fromWCharArray(info.szDevice); panel.monitor = info.rcMonitor; panel.dpi = GetDpiForWindow(window);
        BYTE alpha{}; COLORREF key{}; DWORD flags{};
        if (GetLayeredWindowAttributes(window, &key, &alpha, &flags) && (flags & LWA_ALPHA)) panel.alpha = alpha;
        HRGN region = CreateRectRgn(0,0,0,0);
        if (region) { if (GetWindowRgn(window, region) != ERROR) panel.regionBytes = static_cast<int>(GetRegionData(region,0,nullptr)); DeleteObject(region); }
        reinterpret_cast<std::vector<Panel>*>(context)->push_back(panel); return TRUE;
    };
    for (const auto window : umbrella::platform::taskbarWindows()) inspect(window, reinterpret_cast<LPARAM>(&result));
    std::sort(result.begin(), result.end(), [](const Panel& a, const Panel& b) { return a.device < b.device; });
    return result;
}
QJsonArray snapshot() {
    QJsonArray result;
    for (const auto& panel : panels()) result.append(QJsonObject{{"device", panel.device}, {"x",static_cast<int>(panel.rect.left)}, {"y",static_cast<int>(panel.rect.top)},
        {"width",static_cast<int>(panel.rect.right-panel.rect.left)}, {"height",static_cast<int>(panel.rect.bottom-panel.rect.top)}, {"alpha",panel.alpha}, {"regionBytes",panel.regionBytes}, {"dpi", static_cast<int>(panel.dpi)}});
    return result;
}
bool verify(const QVariantList& monitors, bool plain = false, bool secondaryTop = false) {
    const auto current = panels();
    if (current.size() != static_cast<size_t>(monitors.size())) return false;
    for (const auto& entry : monitors) {
        const auto monitor = entry.toMap(); const bool primary = monitor["primary"].toBool();
        const auto it = std::find_if(current.begin(), current.end(), [&](const Panel& panel) { return panel.device == monitor["device"].toString(); });
        if (it == current.end()) return false;
        const int percent = plain || (secondaryTop && !primary) ? 100 : primary ? 80 : 90;
        const int thickness = plain || (secondaryTop && !primary) ? 48 : primary ? 72 : 64;
        const int transparency = plain ? 0 : primary ? 20 : 40;
        if (abs((it->rect.right-it->rect.left) - MulDiv(it->monitor.right-it->monitor.left,percent,100)) > 2 ||
            abs((it->rect.bottom-it->rect.top) - MulDiv(thickness,static_cast<int>(it->dpi),96)) > 2 ||
            it->alpha != (255*(100-transparency)+50)/100 || (!plain && (primary || !secondaryTop) && it->regionBytes <= 0)) return false;
        if (!plain && secondaryTop && !primary && abs(it->rect.top-it->monitor.top)>2) return false;
    }
    return true;
}
bool waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer timer; timer.start();
    do { if (predicate()) return true; QTest::qWait(30); } while (timer.elapsed()<6000);
    return false;
}
struct LoginBackup {
    QSettings run{QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat};
    bool present = run.contains("UmbrellaShell"); QVariant value = run.value("UmbrellaShell");
    ~LoginBackup() { if (present) run.setValue("UmbrellaShell",value); else run.remove("UmbrellaShell"); run.sync(); }
};
}
int main(int argc, char** argv) {
    QGuiApplication app(argc,argv);
    app.setOrganizationName("UmbrellaShell"); app.setApplicationName("UmbrellaShell");
    const auto args = app.arguments();
    if (args.size()==5 && args[1]=="--taskbar-watch")
        return umbrella::platform::watchTaskbarAppearance(args[2].toULong(),args[3],args[4]);
    if (args.size()==2 && args[1]=="--snapshot") {
        QTextStream(stdout) << QJsonDocument(QJsonObject{{"monitors",QJsonArray::fromVariantList(TaskbarRuntime::monitors())},{"panels",snapshot()}}).toJson(); return 0;
    }
    if (args.size()!=2 || args[1]!="--exercise") return 2;
    if (GetAsyncKeyState(VK_LBUTTON)&0x8000) return 2;
    LoginBackup login;
    const auto originalHide=umbrella::platform::taskbarAutoHide();
    struct HideRestore { std::optional<bool> value; ~HideRestore() { if(value) umbrella::platform::requestTaskbarAutoHide(*value); } } hide{originalHide};
    QTemporaryDir directory;
    if (!directory.isValid()) return 3;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    QJsonObject report{{"before",snapshot()}};
    const auto monitors=TaskbarRuntime::monitors();
    if (monitors.size()!=2) { QTextStream(stderr)<<"Expected two active monitors\n"; return 3; }
    {
        QSettings seed(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        seed.setValue("edge","bottom"); seed.setValue("detached",false); seed.setValue("transparency",0);
        seed.setValue("widthPercent",100); seed.setValue("thickness",48); seed.setValue("islands",false); seed.setValue("autoHide",false); seed.sync();
    }
    {
        TaskbarSettings model;
        for (const auto& entry:monitors) {
            const auto monitor=entry.toMap(); const bool primary=monitor["primary"].toBool();
            model.selectAppearanceMonitor(monitor["id"].toString());
            model.apply("bottom",true,primary?20:40,false,primary?80:90,primary?72:64,!primary);
            if (!waitFor([&]{return !model.pending();})) return 4;
            QTextStream(stdout)<<"apply: "<<model.status()<<'\n';
        }
        if (!waitFor([&]{return verify(monitors);})) { QTextStream(stdout)<<QJsonDocument(snapshot()).toJson(); return 5; }
        report["applied"]=snapshot();
        QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),QSettings::NativeFormat);
        if (run.value("UmbrellaShell").toString()!=QStringLiteral("\"%1\" --resume-settings").arg(QDir::toNativeSeparators(app.applicationFilePath()))) return 6;
    }
    {
        TaskbarSettings reloaded;
        reloaded.restoreSavedAppearance();
        if (!waitFor([&]{return verify(monitors);})) { QTextStream(stdout)<<reloaded.status()<<'\n'<<QJsonDocument(snapshot()).toJson(); return 7; }
        report["restored"]=snapshot();
        // Monitor selection must read each independent persisted profile.
        for (const auto& entry:monitors) {
            const auto monitor=entry.toMap(); reloaded.selectAppearanceMonitor(monitor["id"].toString());
            if (reloaded.transparency()!=(monitor["primary"].toBool()?20:40)) return 8;
        }
        // Restoration must never rewrite the saved profiles.
        QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        const auto before=saved.value("appearanceProfilesV1").toByteArray();
        reloaded.restoreSavedAppearance();
        saved.sync(); if (saved.value("appearanceProfilesV1").toByteArray()!=before) return 9;
        for (const auto& entry : monitors) if (!entry.toMap()["primary"].toBool()) {
            reloaded.selectAppearanceMonitor(entry.toMap()["id"].toString());
            reloaded.apply("top",false,40,false,100,48,false);
        }
        if (!waitFor([&]{return verify(monitors,false,true);})) { QTextStream(stdout)<<reloaded.status()<<'\n'<<QJsonDocument(snapshot()).toJson(); return 19; }
        reloaded.restoreSavedAppearance();
        if (!waitFor([&]{return verify(monitors,false,true);})) { QTextStream(stdout)<<reloaded.status()<<'\n'<<QJsonDocument(snapshot()).toJson(); return 20; }
        report["independentEdgesRestored"]=snapshot();
        reloaded.resetAppearance();
        saved.sync(); if(saved.value("resumeAppearance",true).toBool()) return 10;
    }
    {
        TaskbarSettings model;
        for (const auto& entry : monitors) {
            model.selectAppearanceMonitor(entry.toMap()["id"].toString());
            model.apply("bottom",false,0,true,100,48,false);
            if (!waitFor([&]{return !model.pending();})) return 16;
        }
        if (!waitFor([&]{return verify(monitors,true) && umbrella::platform::taskbarAutoHide().value_or(false);})) return 17;
    }
    {
        TaskbarSettings model; model.restoreSavedAppearance();
        if (!waitFor([&]{return !model.pending() && umbrella::platform::taskbarAutoHide().value_or(false);})) return 18;
        report["autoHideRestored"]=true;
        model.resetAppearance();
    }
    // Event readiness and duplicate-owner protection use real panels but do not modify them.
    {
        TaskbarRuntime runtime;
        if (!runtime.ownsInstance()) return 11;
        TaskbarRuntime duplicate;
        if (duplicate.ownsInstance()) return 12;
        int ready=0; if (!runtime.start([&]{return ++ready>=3;})) return 13;
        if (!waitFor([&]{return ready==3;})) return 14;
        QTest::qWait(300); if(ready!=3) return 15;
    }
    report["after"]=snapshot(); report["passed"]=true;
    QTextStream(stdout)<<QJsonDocument(report).toJson(); return 0;
}
