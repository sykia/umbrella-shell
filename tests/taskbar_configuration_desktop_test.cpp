// Explicit desktop integration: original Task View, or quiet --tray-only mode.
// Never registered with CTest; do not run in unattended CI.
#include <Windows.h>
#include <objbase.h>
#include <oleauto.h>
#include "app/TaskbarSettings.h"
#include "core/taskbar/TrayOrder.h"
#include "platform/windows/TaskbarWindows.h"
#include "platform/windows/TaskbarRuntime.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlEngine>
#include <QtTest/QTest>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QJsonArray>
#include <cstdio>
#include <string>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace {
QQuickItem* item(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto child : root->childItems()) if (auto found = item(child, name)) return found;
    return nullptr;
}
QPoint center(QQuickItem* value) { return value->mapToScene(QPointF(value->width()/2,value->height()/2)).toPoint(); }
}
int main(int argc, char** argv) {
    if(argc==5 && !std::strcmp(argv[1],"--taskbar-watch")) {
        QCoreApplication watcher(argc,argv);
        const std::wstring sandbox=L"Software\\UmbrellaShell\\ConfigurationTest-"+std::to_wstring(watcher.arguments()[2].toULong());
        HKEY registry{};
        if(RegOpenKeyExW(HKEY_CURRENT_USER,sandbox.c_str(),0,KEY_ALL_ACCESS,&registry)!=ERROR_SUCCESS)return 2;
        if(RegOverridePredefKey(HKEY_CURRENT_USER,registry)!=ERROR_SUCCESS){RegCloseKey(registry);return 2;}
        struct RegistryExit {HKEY key;~RegistryExit(){RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);RegCloseKey(key);}} registryExit{registry};
        return umbrella::platform::watchTaskbarAppearance(watcher.arguments()[2].toULong(),watcher.arguments()[3],watcher.arguments()[4]);
    }
    const bool quietMulti=argc==2 && !std::strcmp(argv[1],"--placement-monitors");
    const bool quietStyles=argc==2 && !std::strcmp(argv[1],"--placement-styles");
    const bool quietPlacement=argc==2 && !std::strcmp(argv[1],"--placement-only");
    const bool quietTray=quietMulti || quietStyles || quietPlacement || (argc==2 && !std::strcmp(argv[1],"--tray-only"));
    if (!quietTray && ((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON)|GetAsyncKeyState(VK_MBUTTON)) & 0x8000)) return 2;
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) { std::fprintf(stderr, "%s\n", message.toUtf8().constData()); });
    QGuiApplication app(argc,argv);
    struct DesktopExit {
        HWND foreground;POINT cursor{};bool restore;
        ~DesktopExit(){if(restore){SetCursorPos(cursor.x,cursor.y);if(IsWindow(foreground))SetForegroundWindow(foreground);}}
    } desktopExit{GetForegroundWindow(),{},!quietTray};
    GetCursorPos(&desktopExit.cursor);
    if (app.arguments().size()!=2) return 2;
    qputenv("QT_QUICK_CONTROLS_STYLE","Basic");
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 3;
    struct ComExit { ~ComExit() { CoUninitialize(); } } comExit;
    ComPtr<IUIAutomation> automation;
    ComPtr<IUIAutomationElement> taskbar,button;
    ComPtr<IUIAutomationCondition> condition;
    const auto primary=FindWindowW(L"Shell_TrayWnd",nullptr);
    if (!primary || FAILED(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))) return 3;
    ComPtr<IUIAutomation2> bounded;
    if (SUCCEEDED(automation.As(&bounded))) { bounded->put_ConnectionTimeout(1000); bounded->put_TransactionTimeout(1500); }
    VARIANT id{}; id.vt=VT_BSTR; id.bstrVal=SysAllocString(L"TaskViewButton");
    const auto hr=automation->CreatePropertyCondition(UIA_AutomationIdPropertyId,id,&condition); VariantClear(&id);
    if (FAILED(hr)) return 3;
    QElapsedTimer providerReady; providerReady.start();
    do {
        taskbar.Reset(); button.Reset();
        if (SUCCEEDED(automation->ElementFromHandle(primary,&taskbar)) &&
            SUCCEEDED(taskbar->FindFirst(TreeScope_Descendants,condition.Get(),&button)) && button) break;
        QTest::qWait(30);
    } while (providerReady.elapsed()<3000);
    if (!button) { std::puts("UIA: original Task View did not become ready within 3 seconds"); return 3; }
    RECT before{}; if (FAILED(button->get_CurrentBoundingRectangle(&before))) return 3;
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) return 4;
    const auto realProfiles=QSettings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar").value("appearanceProfilesV1").toByteArray();
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
    // Redirect HKCU for this test process. The actual Run key is never written,
    // even while exercising auto-save and restart replay.
    struct RegistrySandbox {
        HKEY key{};
        std::wstring path = L"Software\\UmbrellaShell\\ConfigurationTest-" + std::to_wstring(GetCurrentProcessId());
        bool active = false;
        RegistrySandbox() {
            if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key, nullptr) == ERROR_SUCCESS) {
                HKEY advanced{};
                if(RegCreateKeyExW(key,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",0,nullptr,0,KEY_ALL_ACCESS,nullptr,&advanced,nullptr)!=ERROR_SUCCESS)return;
                RegCloseKey(advanced);
                active = RegOverridePredefKey(HKEY_CURRENT_USER, key) == ERROR_SUCCESS;
            }
        }
        ~RegistrySandbox() {
            if (active) RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
            if (key) { RegCloseKey(key); RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); }
        }
    } registrySandbox;
    if (!registrySandbox.active) return 4;
    if(quietStyles) {
        const auto signature=[](const QString& device) {
            QByteArray result;
            for(const auto window:umbrella::platform::taskbarWindows()) {
                MONITORINFOEXW monitor{};monitor.cbSize=sizeof(monitor);RECT rect{};
                if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONULL),&monitor) || device!=QString::fromWCharArray(monitor.szDevice) || !GetWindowRect(window,&rect))continue;
                BYTE alpha=255;DWORD flags{};COLORREF key{};GetLayeredWindowAttributes(window,&key,&alpha,&flags);
                const auto add=[&](LONG value){result+=QByteArray::number(value)+',';};
                for(const auto value:{rect.left,rect.top,rect.right,rect.bottom,monitor.rcWork.left,monitor.rcWork.top,monitor.rcWork.right,monitor.rcWork.bottom})add(value);
                result+=QByteArray::number(alpha)+':';
                HRGN region=CreateRectRgn(0,0,0,0);
                if(region) {
                    if(GetWindowRgn(window,region)!=ERROR) {
                        const auto size=GetRegionData(region,0,nullptr);
                        if(size && size<=65536) {
                            QByteArray bytes(static_cast<qsizetype>(size),0);
                            GetRegionData(region,size,reinterpret_cast<RGNDATA*>(bytes.data()));result+=bytes.toHex();
                        }
                    }
                    DeleteObject(region);
                }
            }
            return result;
        };
        const auto monitors=umbrella::platform::TaskbarRuntime::monitors();
        if(monitors.size()!=2)return 77;
        QString primaryId;QVariantMap layouts;QByteArray committed;
        {
            TaskbarSettings model;
            for(const auto& entry:monitors) {
                const auto monitor=entry.toMap();const auto id=monitor["id"].toString();
                const auto present=model.componentIds(id,true);if(present.isEmpty())return 80;
                if(monitor["primary"].toBool())primaryId=id;
                QVariantMap layout{{"version",1},{"gap",8}};QStringList available,left,center,right;
                const auto catalogue=QStringLiteral("widgets start search taskview apps overflow input network volume clock desktop trayapps appoverflow power pen keyboard touchpad emoji studio microphone callmute privacy notifications eject vpn chat copilot systemtray remote").split(' ');
                for(const auto& role:catalogue) {
                    if(!present.contains(role)){available.append(role);continue;}
                    if(role=="widgets" || role=="taskview")left.append(role);
                    else if(role=="start" || role=="search" || role=="apps")center.append(role);
                    else if(role!="desktop")right.append(role);
                }
                if(present.contains("desktop"))right.append("desktop");
                layout["available"]=available;layout["left"]=left;layout["center"]=center;layout["right"]=right;
                if(!model.applyComponentConfiguration(id,layout)){std::fprintf(stderr,"Initial styled placement: %s\n",model.status().toUtf8().constData());return 81;}
                layouts.insert(id,layout);
            }
            QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            committed=saved.value("confirmedComponentsV2").toByteArray();
            for(int mode=0;mode<3;++mode)for(const auto& entry:monitors) {
                const auto monitor=entry.toMap();const auto id=monitor["id"].toString();
                QMap<QString,QByteArray> peers;
                for(const auto& other:monitors)if(other.toMap()["id"]!=monitor["id"])
                    peers.insert(other.toMap()["device"].toString(),signature(other.toMap()["device"].toString()));
                model.selectAppearanceMonitor(id);
                model.apply("bottom",mode!=0,0,false,mode?80:100,mode?64:48,mode==2);
                QTest::qWait(550);
                if(!model.savedAppearanceRestored() || model.activeComponentConfigurations()!=layouts ||
                    model.detached()!=(mode!=0) || model.islands()!=(mode==2) || model.thickness()!=(mode?64:48)) {
                    std::fprintf(stderr,"Style %d: %s\n",mode,model.status().toUtf8().constData());return 82;
                }
                saved.sync();
                if(saved.value("confirmedComponentsV2").toByteArray()!=committed)return 83;
                for(auto peer=peers.begin();peer!=peers.end();++peer)if(signature(peer.key())!=peer.value())return 84;
                RECT moved{};if(FAILED(button->get_CurrentBoundingRectangle(&moved)))return 85;
                for(const auto window:umbrella::platform::taskbarWindows()) {
                    MONITORINFOEXW info{};info.cbSize=sizeof(info);RECT panel{};
                    if(!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONULL),&info) || !GetWindowRect(window,&panel))return 85;
                    if(QString::fromWCharArray(info.szDevice)==monitor["device"].toString()) {
                        const auto dpi=GetDpiForWindow(window);
                        if(std::abs(panel.right-panel.left-MulDiv(info.rcMonitor.right-info.rcMonitor.left,mode?80:100,100))>2 ||
                            std::abs(panel.bottom-panel.top-MulDiv(mode?64:48,static_cast<int>(dpi),96))>2)return 85;
                    }
                    if(!(info.dwFlags&MONITORINFOF_PRIMARY))continue;
                    HRGN region=CreateRectRgn(0,0,0,0);
                    const auto kind=GetWindowRgn(window,region);
                    const bool reachable=kind==ERROR || PtInRegion(region,(moved.left+moved.right)/2-panel.left,(moved.top+moved.bottom)/2-panel.top);
                    DeleteObject(region);if(!reachable)return 86;
                }
            }
        }
        QTest::qWait(550);
        {
            TaskbarSettings restarted;
            if(!restarted.restoreSavedAppearance() || !restarted.savedAppearanceRestored())return 87;
            QTest::qWait(550);
            if(QJsonObject::fromVariantMap(restarted.activeComponentConfigurations())!=QJsonObject::fromVariantMap(layouts)) {
                std::fprintf(stderr,"Restart layout: %s; actual=%s expected=%s\n",restarted.status().toUtf8().constData(),
                    QJsonDocument(QJsonObject::fromVariantMap(restarted.activeComponentConfigurations())).toJson(QJsonDocument::Compact).constData(),
                    QJsonDocument(QJsonObject::fromVariantMap(layouts)).toJson(QJsonDocument::Compact).constData());return 88;
            }
            const auto actualProfiles=QJsonDocument::fromJson(realProfiles).object();
            if(!actualProfiles.isEmpty() && realProfiles.size()<=65536) {
                QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
                saved.setValue("appearanceProfilesV1",realProfiles);saved.setValue("resumeAppearance",true);saved.sync();
                if(!restarted.restoreSavedAppearance() || !restarted.savedAppearanceRestored()) {
                    std::fprintf(stderr,"Real saved profile clone: %s\n",restarted.status().toUtf8().constData());return 89;
                }
                QTest::qWait(550);if(QJsonObject::fromVariantMap(restarted.activeComponentConfigurations())!=QJsonObject::fromVariantMap(layouts))return 90;
                std::puts("Passed: cloned real saved appearance profiles, original settings unchanged.");
            }
            const auto secondary=monitors[0].toMap()["id"].toString()==primaryId?monitors[1].toMap():monitors[0].toMap();
            const auto peer=signature(secondary["device"].toString());
            if(!restarted.restoreComponentConfiguration(primaryId) || signature(secondary["device"].toString())!=peer)return 91;
        }
        std::puts("Passed: same confirmed layout across fused/detached/islands, independent peers, persisted restart and scoped reset.");
        return 0;
    }
    if(quietMulti) {
        const auto snapshot=[]() {
            QHash<QString,QVariantMap> result;QProcess probe;
            probe.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* value){value->flags|=CREATE_NO_WINDOW;});
            probe.start(QCoreApplication::applicationDirPath()+"/UmbrellaTaskbarHost.exe",{"--inspect-components"});
            if(!probe.waitForFinished(4000) || probe.exitCode())return result;
            QString device;
            const QRegularExpression item(QStringLiteral("^  (start|clock|systemtray) .*bounds=\\(([^)]+)\\) visible=1"));
            for(const auto& line:QString::fromUtf8(probe.readAllStandardOutput()).split('\n')) {
                if(line.startsWith("components-monitor "))device=line.mid(19).trimmed();
                const auto match=item.match(line);
                if(!device.isEmpty() && match.hasMatch())result[device].insert(match.captured(1),match.captured(2));
            }
            return result;
        };
        const auto beforeAll=snapshot();if(beforeAll.size()!=2)return 77;
        QString primaryId,secondaryId,primaryDevice,secondaryDevice;QVariantMap primaryProfile,secondaryProfile;
        const auto profile=[](const QStringList& present,bool primary) {
            QVariantMap value{{"version",1},{"gap",8}};QJsonArray available,left,center,right;
            const auto catalogue=QStringLiteral("widgets start search taskview apps overflow input network volume clock desktop trayapps appoverflow power pen keyboard touchpad emoji studio microphone callmute privacy notifications eject vpn chat copilot systemtray remote").split(' ');
            for(const auto& role:catalogue) {
                if(!present.contains(role)){available.append(role);continue;}
                if(role=="widgets" || (primary && (role=="start" || role=="systemtray")) || (!primary && role=="clock"))left.append(role);
                else if(role=="apps" || (primary && (role=="search" || role=="taskview")))center.append(role);
                else if(role!="desktop")right.append(role);
            }
            if(present.contains("desktop"))right.append("desktop");
            value["available"]=available.toVariantList();value["left"]=left.toVariantList();value["center"]=center.toVariantList();value["right"]=right.toVariantList();return value;
        };
        {
            TaskbarSettings model;
            for(const auto& entry:model.layoutMonitors()) {
                const auto monitor=entry.toMap();const auto id=monitor["id"].toString();const auto ids=model.componentIds(id);
                if(ids.isEmpty())return 30;
                if(monitor["primary"].toBool()){primaryId=id;primaryDevice=monitor["device"].toString();primaryProfile=profile(ids,true);}
                else {secondaryId=id;secondaryDevice=monitor["device"].toString();secondaryProfile=profile(ids,false);}
            }
            if(primaryId.isEmpty() || secondaryId.isEmpty())return 77;
            // A disabled historical islands profile must survive component-only
            // editing without being silently re-enabled during restart replay.
            QJsonObject disabledProfiles;
            const QJsonObject disabled{{"edge","bottom"},{"detached",true},{"widthPercent",100},{"thickness",64},{"islands",true},{"transparency",0}};
            disabledProfiles.insert(primaryId,disabled);disabledProfiles.insert(secondaryId,disabled);
            QSettings previous(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            previous.setValue("appearanceProfilesV1",QJsonDocument(disabledProfiles).toJson(QJsonDocument::Compact));previous.setValue("resumeAppearance",false);previous.sync();
            if(!model.applyComponentConfiguration(primaryId,primaryProfile) || !model.applyComponentConfiguration(secondaryId,secondaryProfile)){
                std::fprintf(stderr,"Two-panel placement: %s\n",model.status().toUtf8().constData());return 31;
            }
            QTest::qWait(1300);
            if(model.activeComponentConfigurations().size()!=2)return 32;
            const auto moved=snapshot();
            const auto x=[](const QVariantMap& panel,const QString& role){return panel[role].toString().section(',',0,0).toDouble();};
            if(!moved.contains(primaryDevice) || !moved.contains(secondaryDevice) || x(moved[primaryDevice],"start")>=400 || x(moved[secondaryDevice],"clock")>=400)return 33;
            if(primaryProfile["left"].toList().contains("systemtray") && x(moved[primaryDevice],"systemtray")>=400)return 34;
            QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            if(QJsonDocument::fromJson(saved.value("confirmedComponentsV2").toByteArray()).object().size()!=2)return 35;
            if(saved.value("resumeAppearance",true).toBool() || !saved.value("resumeComponents",false).toBool() || QJsonDocument::fromJson(saved.value("appearanceProfilesV1").toByteArray()).object()!=disabledProfiles)return 35;
        }
        QTest::qWait(1300);if(snapshot()!=beforeAll)return 36;
        {
            TaskbarSettings restarted;
            if(!restarted.restoreSavedAppearance()) {std::fprintf(stderr,"Two-panel replay: %s\n",restarted.status().toUtf8().constData());return 37;}
            QTest::qWait(1300);if(restarted.activeComponentConfigurations().size()!=2)return 38;
            const auto moved=snapshot();
            if(!restarted.restoreComponentConfiguration(primaryId))return 39;
            QTest::qWait(1300);
            if(snapshot().value(secondaryDevice)!=moved.value(secondaryDevice) || restarted.activeComponentConfigurations().size()!=1 || restarted.componentDraft(secondaryId)!=secondaryProfile)return 40;
            QSettings saved(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            const auto records=QJsonDocument::fromJson(saved.value("confirmedComponentsV2").toByteArray()).object();
            if(records.size()!=1 || !records.contains(secondaryId))return 41;
            if(!restarted.restoreComponentConfiguration(secondaryId))return 42;
        }
        QTest::qWait(1300);if(snapshot()!=beforeAll)return 43;
        std::puts("Passed: actual two-monitor placement, detected original groups, v2 save/replay, isolated reset and original geometry restoration; registry/settings sandbox, no input.");
        return 0;
    }
    if(quietTray) {
        // Quiet end-to-end mode: no view, SendInput, SetFocus or flyout invocation.
        // Both preferences and Run-key writes remain inside the sandboxes above.
        const auto snapshot=[&]() {
            QProcess probe;
            probe.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* value){value->flags|=CREATE_NO_WINDOW;});
            probe.start(QCoreApplication::applicationDirPath()+"/UmbrellaTaskbarHost.exe",{"--inspect-components"});
            if(!probe.waitForFinished(5000) || probe.exitCode()!=0)return QString();
            const QString output=QString::fromUtf8(probe.readAllStandardOutput());
            QString result;
            for(const auto& role:{QStringLiteral("overflow"),QStringLiteral("input"),QStringLiteral("clock"),QStringLiteral("quicksettings")}) {
                const auto match=QRegularExpression("^  "+role+" .*?bounds=\\(([^)]+)\\) visible=1",QRegularExpression::MultilineOption).match(output);
                if(!match.hasMatch())return QString();
                result+=role+":"+match.captured(1)+";";
            }
            return result;
        };
        const auto foreground=GetForegroundWindow();
        const auto original=snapshot();if(original.isEmpty())return 11;
        auto configuration=QJsonDocument::fromJson(R"({"version":1,"gap":8,
            "left":["widgets"],"center":["start","search","taskview","apps"],
            "right":["clock","network","volume","input","overflow","desktop"],
            "available":["trayapps","appoverflow","power","pen","keyboard","touchpad","emoji","studio","microphone","callmute","privacy","notifications","eject","vpn","chat","copilot","systemtray","remote"]})").object().toVariantMap();
        QString monitor;
        if(quietPlacement) {
            configuration["left"]=QStringList{"widgets","start"};
            configuration["center"]=QStringList{"search","taskview","apps"};
            configuration["right"]=QStringList{"overflow","input","network","volume","clock","desktop"};
            configuration=QJsonObject::fromVariantMap(configuration).toVariantMap();
        }
        {
            TaskbarSettings model;
            for(const auto& item:model.layoutMonitors())if(item.toMap()["primary"].toBool())monitor=item.toMap()["id"].toString();
            // Start the ordinary appearance owner at the existing native size.
            // The real tray apply must share it, rather than replacing its host.
            model.apply("bottom",false,0,false,100,48,false);
            QSettings profile(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
            if(!profile.contains("appearanceProfilesV1")){std::fprintf(stderr,"Appearance setup: %s\n",model.status().toUtf8().constData());return 19;}
            QElapsedTimer elapsed;elapsed.start();
            for(unsigned code=0;code<(quietPlacement?0u:24u);++code) {
                const auto order=*umbrella::taskbar::trayOrder(code);
                QStringList right;
                const std::array<QStringList,4> ids{QStringList{"overflow"},QStringList{"input"},QStringList{"network","volume"},QStringList{"clock"}};
                for(const auto role:order)right.append(ids[role]);
                right.append("desktop");
                auto candidate=configuration;candidate["right"]=right;
                if(!model.applyComponentConfiguration(monitor,candidate)) {
                    std::fprintf(stderr,"Permutation %u: %s\n",code,model.status().toUtf8().constData());return 20;
                }
                const auto measured=snapshot();if(measured.isEmpty())return 21;
                double last=-1;
                for(const auto role:order) {
                    const auto componentId=role==2 ? QStringLiteral("quicksettings") : ids[role].front();
                    const auto match=QRegularExpression(componentId+":([-0-9.]+),").match(measured);
                    bool valid=false;const double x=match.captured(1).toDouble(&valid);
                    if(!valid || x<=last)return 22;
                    last=x;
                }
            }
            if(!quietPlacement)std::printf("All 24 original tray group orders verified through native slots and sandboxed persistence in %lld ms (includes diagnostic IPC).\n",elapsed.elapsed());
            if(!model.applyComponentConfiguration(monitor,configuration)){std::fprintf(stderr,"Tray apply: %s\n",model.status().toUtf8().constData());return 12;}
            if(quietPlacement)QTest::qWait(250); // Observe any late native rejection before checking persistence.
            if(model.activeComponentConfiguration()!=configuration || model.componentDraft(monitor)!=configuration || snapshot()==original)return 13;
            auto invalid=configuration;invalid["right"]=QStringList{"overflow","input","network","clock","volume","desktop"};
            if(model.applyComponentConfiguration(monitor,invalid) || model.componentDraft(monitor)!=configuration)return 14;
        }
        if(quietPlacement)QTest::qWait(250); // A real restart follows owner teardown and native closing animations.
        if(snapshot()!=original)return 15;
        {
            TaskbarSettings restarted;
            if(!restarted.restoreSavedAppearance() || restarted.activeComponentConfiguration()!=configuration || snapshot()==original)return 16;
            if(!restarted.restoreComponentConfiguration(monitor) || snapshot()!=original)return 17;
        }
        QSettings state(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
        if(state.contains("confirmedComponentV1") || state.value("resumeComponents",true).toBool() || !state.value("resumeAppearance",false).toBool() || GetForegroundWindow()!=foreground){
            std::printf("Tray reset diagnostics: confirmed=%u resume=%u foreground=%u\n",state.contains("confirmedComponentV1"),state.value("resumeAppearance",true).toBool(),GetForegroundWindow()==foreground);return 18;
        }
        std::puts("Passed: real component placement through TaskbarSettings, sandboxed save/reload/restart replay, split-group refusal, reset and owner shutdown; no view/input/flyout or actual Run-key changes.");
        return 0;
    }
    QVariantMap confirmedForRestart;
    QString confirmedMonitor;
    {
    TaskbarSettings model; // Its destructor and the host owner-death worker keep rollback armed.
    QQuickView view;
    bool warning=false;
    QObject::connect(view.engine(),&QQmlEngine::warnings,&app,[&] { warning=true; });
    view.setInitialProperties({{"system",QVariant::fromValue(&model)}});
    view.setSource(QUrl::fromLocalFile(app.arguments()[1]));
    if (view.status()!=QQuickView::Ready) return 4;
    view.setResizeMode(QQuickView::SizeRootObjectToView); view.resize(900,1000); view.setPosition(400,100); view.show();
    if (!QTest::qWaitForWindowExposed(&view,3000)) return 4;
    QTest::qWait(250);
    if (FAILED(button->get_CurrentBoundingRectangle(&before))) return 3;
    auto root=view.rootObject();
    const auto detectedMonitors=model.layoutMonitors();
    for (qsizetype i=0; i<detectedMonitors.size(); ++i)
        if (detectedMonitors[i].toMap().value("primary").toBool()) root->setProperty("monitorIndex", static_cast<int>(i));
    QTest::qWait(100);
    auto source=item(root,"component_taskview"), zone=item(root,"configuration_left"), reset=item(root,"configurationReset");
    if (!source || !zone || !reset) return 4;
    const QPoint start=center(source), target=zone->mapToScene(QPointF(zone->width()/2,105)).toPoint();
    QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,start);
    for (int i=1;i<=12;++i) { QTest::mouseMove(&view,start+(target-start)*i/12); QTest::qWait(20); }
    QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,target); QTest::qWait(1300);
    const auto monitors=model.layoutMonitors();
    const auto primaryId=model.activeComponentMonitor();
    const auto saved=model.componentDraft(primaryId);
    if (saved.isEmpty()) { std::printf("draft: missing; %s\n",root->property("feedback").toString().toUtf8().constData()); std::printf("backend: %s\n",model.status().toUtf8().constData()); return 5; }
    TaskbarSettings reloadedModel;
    if (reloadedModel.componentDraft(primaryId)!=saved) { std::puts("draft: model reload mismatch"); return 5; }
    QQuickView reloadedView;
    reloadedView.setInitialProperties({{"system",QVariant::fromValue(&reloadedModel)}});
    reloadedView.setSource(QUrl::fromLocalFile(app.arguments()[1]));
    // Loading saved preferences into a new model does not claim native application.
    if (reloadedView.status()!=QQuickView::Ready || !reloadedModel.activeComponentConfiguration().isEmpty()) return 5;
    for (const auto& entry : monitors) if (!entry.toMap().value("primary").toBool()) {
        const auto secondaryId=entry.toMap().value("id").toString();
        auto other=saved; other.insert("gap",24);
        if (!model.saveComponentDraft(secondaryId,other) || model.componentDraft(primaryId)!=saved ||
            reloadedModel.componentDraft(secondaryId).value("gap").toInt()!=24) return 5;
    }
    auto invalid=saved; invalid.insert("gap",10000);
    if (model.saveComponentDraft(primaryId,invalid) || model.componentDraft(primaryId)!=saved) return 5;
    std::printf("settings drafts: saved/reloaded=1 monitorIsolation=1 monitors=%lld invalidRejected=1\n",static_cast<long long>(monitors.size()));
    // Release is the only user action; auto-apply has already acknowledged and saved.
    if (model.activeComponentConfiguration()!=saved) return 5;
    auto unsupported=saved; unsupported.insert("version",2);
    if (model.applyComponentConfiguration(primaryId,unsupported) || model.componentDraft(primaryId)!=saved) return 5;
    std::printf("auto apply: available=%u status=%s\n",root->property("nativeAvailable").toBool()?1u:0u,model.status().toUtf8().constData());
    RECT moved{},panel{};
    const bool movedOk=SUCCEEDED(button->get_CurrentBoundingRectangle(&moved)) && GetWindowRect(primary,&panel) &&
        moved.left>=panel.left && moved.left<before.left-100 && moved.top==before.top && moved.right-moved.left==before.right-before.left;
    BOOL enabled{}; button->get_CurrentIsEnabled(&enabled);
    // Bring the real taskbar above a covering fullscreen app before point-hit testing.
    SetForegroundWindow(primary); button->SetFocus(); QTest::qWait(150);
    // Exercise native pointer hit testing, after the Qt-only drag in settings.
    POINT cursor{}; GetCursorPos(&cursor);
    INPUT pointer{}; pointer.type=INPUT_MOUSE;
    pointer.mi.dx=MulDiv((moved.left+moved.right)/2-GetSystemMetrics(SM_XVIRTUALSCREEN),65535,GetSystemMetrics(SM_CXVIRTUALSCREEN)-1);
    pointer.mi.dy=MulDiv((moved.top+moved.bottom)/2-GetSystemMetrics(SM_YVIRTUALSCREEN),65535,GetSystemMetrics(SM_CYVIRTUALSCREEN)-1);
    pointer.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    SendInput(1,&pointer,sizeof(pointer)); QTest::qWait(150);
    ComPtr<IUIAutomationElement> hit; BOOL same{};
    if (SUCCEEDED(automation->ElementFromPoint({(moved.left+moved.right)/2,(moved.top+moved.bottom)/2},&hit))) {
        ComPtr<IUIAutomationTreeWalker> walker; automation->get_RawViewWalker(&walker);
        for (int i=0;hit && walker && i<10;++i) {
            if (SUCCEEDED(automation->CompareElements(hit.Get(),button.Get(),&same)) && same) break;
            ComPtr<IUIAutomationElement> parent; if (FAILED(walker->GetParentElement(hit.Get(),&parent))) break; hit=parent;
        }
    }
    wchar_t hitClass[128]{}; const auto hitRoot=GetAncestor(WindowFromPoint({(moved.left+moved.right)/2,(moved.top+moved.bottom)/2}),GA_ROOT); GetClassNameW(hitRoot,hitClass,128);
    BSTR hitName=nullptr; if (hit) hit->get_CurrentClassName(&hitName);
    std::printf("hit diagnostic: window=%ls sameRoot=%u uiaClass=%ls\n",hitClass,hitRoot==primary?1u:0u,hitName?hitName:L"none"); SysFreeString(hitName);
    std::printf("settings integration: automaticApplyAndSave=1 moved=%u x=%ld originalHit=%u enabled=%u\n",movedOk?1u:0u,moved.left,same?1u:0u,enabled?1u:0u);
    bool nativeClick=false;
    if (movedOk && same && hitRoot==primary) {
        INPUT clicks[2]{}; clicks[0].type=clicks[1].type=INPUT_MOUSE;
        clicks[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; clicks[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if (SendInput(2,clicks,sizeof(INPUT))==2) {
            QTest::qWait(700);
            wchar_t surface[128]{}; GetClassNameW(GetForegroundWindow(),surface,128);
            nativeClick=!wcscmp(surface,L"XamlExplorerHostIslandWindow") || !wcscmp(surface,L"MultitaskingViewFrame");
            if (nativeClick) {
                INPUT keys[2]{}; keys[0].type=keys[1].type=INPUT_KEYBOARD;
                keys[0].ki.wVk=keys[1].ki.wVk=VK_ESCAPE; keys[1].ki.dwFlags=KEYEVENTF_KEYUP;
                SendInput(2,keys,sizeof(INPUT)); QTest::qWait(500);
            }
        }
    }
    std::printf("settings integration: originalTaskViewClick=%u\n",nativeClick?1u:0u);
    SetCursorPos(cursor.x,cursor.y);
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,center(reset)); QTest::qWait(1300);
    RECT after{};
    const bool restored=model.componentDraft(primaryId).isEmpty() && SUCCEEDED(button->get_CurrentBoundingRectangle(&after)) && EqualRect(&before,&after);
    std::printf("settings integration: reset=%u warnings=%u\n",restored?1u:0u,warning?1u:0u);
    if (!(movedOk && same && enabled && nativeClick && restored && !warning)) return 1;
    if (!model.applyComponentConfiguration(primaryId,saved)) return 6;
    confirmedForRestart=saved; confirmedMonitor=primaryId;
    }
    // A new owner may replay only the native-confirmed scenario, not arbitrary drafts.
    QTest::qWait(1300);
    TaskbarSettings restarted;
    if (!restarted.restoreSavedAppearance() || restarted.activeComponentConfiguration()!=confirmedForRestart ||
        restarted.activeComponentMonitor()!=confirmedMonitor) return 7;
    QTest::qWait(1300);
    RECT replayed{};
    if (FAILED(button->get_CurrentBoundingRectangle(&replayed)) || replayed.left==before.left) return 8;
    if (!restarted.restoreComponentConfiguration(confirmedMonitor)) return 9;
    QTest::qWait(1300);
    RECT resetReplay{};if(FAILED(button->get_CurrentBoundingRectangle(&resetReplay)) || !EqualRect(&before,&resetReplay))return 9;
    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"UmbrellaShell","Taskbar");
    if (settings.value("resumeAppearance",true).toBool() || settings.contains("confirmedComponentV1")) return 10;
    std::puts("settings integration: confirmedRestartReplay=1 explicitResetDisablesResume=1");
    return 0;
}
