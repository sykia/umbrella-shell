// Explicit desktop integration test: moves ONE original Task View from settings.
// Never registered with CTest; do not run in unattended CI.
#include <Windows.h>
#include <objbase.h>
#include <oleauto.h>
#include "app/TaskbarSettings.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlEngine>
#include <QtTest/QTest>
#include <cstdio>
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
    if ((GetAsyncKeyState(VK_LBUTTON)|GetAsyncKeyState(VK_RBUTTON)|GetAsyncKeyState(VK_MBUTTON)) & 0x8000) return 2;
    QGuiApplication app(argc,argv);
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
    if (FAILED(hr) || FAILED(automation->ElementFromHandle(primary,&taskbar)) ||
        FAILED(taskbar->FindFirst(TreeScope_Descendants,condition.Get(),&button)) || !button) return 3;
    RECT before{}; if (FAILED(button->get_CurrentBoundingRectangle(&before))) return 3;
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) return 4;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
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
    auto source=item(root,"component_taskview"), zone=item(root,"configuration_left"), apply=item(root,"configurationApply"), reset=item(root,"configurationReset");
    if (!source || !zone || !apply || !reset) return 4;
    const QPoint start=center(source), target=zone->mapToScene(QPointF(zone->width()/2,105)).toPoint();
    QTest::mousePress(&view,Qt::LeftButton,Qt::NoModifier,start);
    for (int i=1;i<=12;++i) { QTest::mouseMove(&view,start+(target-start)*i/12); QTest::qWait(20); }
    QTest::mouseRelease(&view,Qt::LeftButton,Qt::NoModifier,target); QTest::qWait(220);
    const auto monitors=model.layoutMonitors();
    const auto primaryId=monitors.front().toMap().value("id").toString();
    const auto saved=model.componentDraft(primaryId);
    if (saved.isEmpty()) { std::printf("draft: missing; %s\n",root->property("feedback").toString().toUtf8().constData()); return 5; }
    TaskbarSettings reloadedModel;
    if (reloadedModel.componentDraft(primaryId)!=saved) { std::puts("draft: model reload mismatch"); return 5; }
    QQuickView reloadedView;
    reloadedView.setInitialProperties({{"system",QVariant::fromValue(&reloadedModel)}});
    reloadedView.setSource(QUrl::fromLocalFile(app.arguments()[1]));
    if (reloadedView.status()!=QQuickView::Ready || reloadedView.rootObject()->property("applyScenario").toString()!="after-widgets") { std::printf("draft: QML reload scenario=%s\n",reloadedView.rootObject()->property("applyScenario").toString().toUtf8().constData()); return 5; }
    if (monitors.size()>1) {
        const auto secondaryId=monitors[1].toMap().value("id").toString();
        auto other=saved; other.insert("gap",24);
        if (!model.saveComponentDraft(secondaryId,other) || model.componentDraft(primaryId)!=saved ||
            reloadedModel.componentDraft(secondaryId).value("gap").toInt()!=24) return 5;
    }
    auto invalid=saved; invalid.insert("gap",10000);
    if (model.saveComponentDraft(primaryId,invalid) || model.componentDraft(primaryId)!=saved) return 5;
    std::printf("settings drafts: saved/reloaded=1 monitorIsolation=1 monitors=%lld invalidRejected=1\n",static_cast<long long>(monitors.size()));
    // No native request until the separate Apply button is clicked.
    RECT draft{}; if (FAILED(button->get_CurrentBoundingRectangle(&draft)) || !EqualRect(&draft,&before)) return 5;
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,center(apply)); QTest::qWait(250);
    std::printf("apply: enabled=%u available=%u status=%s\n",apply->isEnabled()?1u:0u,root->property("nativeAvailable").toBool()?1u:0u,model.status().toUtf8().constData());
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
    std::printf("settings integration: draftNoWrite=1 moved=%u x=%ld originalHit=%u enabled=%u\n",movedOk?1u:0u,moved.left,same?1u:0u,enabled?1u:0u);
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
    QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,center(reset)); QTest::qWait(250);
    RECT after{};
    const bool restored=SUCCEEDED(button->get_CurrentBoundingRectangle(&after)) && EqualRect(&before,&after);
    std::printf("settings integration: reset=%u warnings=%u\n",restored?1u:0u,warning?1u:0u);
    return movedOk && same && enabled && nativeClick && restored && !warning ? 0 : 1;
}
