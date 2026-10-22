"""Qt pointer-event integration tests for the configuration editor; no Windows writes.
Run with PySide6 6.8.3. Offscreen QQuickView exercises real MouseArea capture/layout.
"""
import os
from pathlib import Path
os.environ["QT_QUICK_CONTROLS_STYLE"] = "Basic"
from PySide6.QtCore import QObject, Property, Signal, Slot, QUrl, QPoint, Qt
from PySide6.QtGui import QGuiApplication, QColor, QFontDatabase
from PySide6.QtQuick import QQuickView, QQuickItem
from PySide6.QtTest import QTest

class Backend(QObject):
    changed = Signal()
    def __init__(self):
        super().__init__()
        self.calls = []
        self.reset_ok = True
        self.drafts = {}
        self.active = {}
        self.active_monitor = ""
        self.apply_ok = True
        self.status_text = ""
        self.busy = False
    layoutMonitors = Property("QVariantList", lambda self: [
        {"id":"one","label":"Основной","primary":True,"width":2560},
        {"id":"two","label":"Второй","primary":False,"width":2560}], constant=True)
    componentPlacementAvailable = Property(bool, lambda self: True, constant=True)
    pending = Property(bool, lambda self: self.busy, notify=changed)
    activeComponentConfiguration = Property("QVariantMap", lambda self: self.active, notify=changed)
    activeComponentMonitor = Property(str, lambda self: self.active_monitor, notify=changed)
    status = Property(str, lambda self: self.status_text, notify=changed)
    @Slot(str, "QVariantMap", result=bool)
    def applyComponentConfiguration(self, monitor, configuration):
        self.calls.append((monitor, configuration["gap"]))
        if not self.apply_ok: return False
        self.active = configuration
        self.active_monitor = monitor
        self.drafts[monitor] = configuration
        self.changed.emit()
        return True
    @Slot(str, result=bool)
    def restoreComponentConfiguration(self, monitor):
        self.calls.append(("reset", monitor))
        if not self.reset_ok: return False
        if self.active_monitor == monitor:
            self.active = {}; self.active_monitor = ""
        self.drafts.pop(monitor, None)
        self.changed.emit()
        return True
    @Slot(str, result="QVariantMap")
    def componentDraft(self, monitor):
        return self.drafts.get(monitor,{})
    @Slot(str,"QVariantMap",result=bool)
    def saveComponentDraft(self, monitor, draft):
        self.drafts[monitor] = draft
        return True
    @Slot(str, int, result=bool)
    def placeTaskViewAfterWidgets(self, monitor, x):
        self.calls.append((monitor,x)); return True
    @Slot(result=bool)
    def resetComponentLayout(self):
        self.calls.append("reset"); return self.reset_ok

def require(value, message):
    if not value: raise RuntimeError(message)

def main():
    app = QGuiApplication(["configuration-test", "-platform", "offscreen"])
    QFontDatabase.addApplicationFont("C:/Windows/Fonts/segoeui.ttf")
    QFontDatabase.addApplicationFont("C:/Windows/Fonts/segmdl2.ttf")
    backend = Backend()
    view = QQuickView()
    view.setColor(QColor("#202329"))
    warnings = []
    view.engine().warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
    view.setInitialProperties({"system":backend})
    view.setSource(QUrl.fromLocalFile(str(Path(__file__).resolve().parents[1]/"src/presentation/TaskbarConfiguration.qml")))
    require(view.status() == QQuickView.Ready, str(view.errors()))
    root = view.rootObject()
    root.setWidth(900)
    root.setHeight(root.implicitHeight())
    view.resize(900,1100); view.show(); QTest.qWait(150)
    def config(): return root.property("configuration").toVariant()
    def item(name):
        def walk(node):
            if node.objectName() == name: return node
            for child in node.childItems():
                found = walk(child)
                if found is not None: return found
            return None
        found = walk(root)
        require(found is not None, "missing " + name)
        return found
    def center(obj):
        p = obj.mapToScene(obj.boundingRect().center())
        return QPoint(round(p.x()),round(p.y()))
    def drag(id, zone, index=0):
        source = center(item("component_"+id))
        dest = item("configuration_"+zone) if zone != "available" else item("configurationAvailable")
        origin = dest.mapToScene(dest.boundingRect().topLeft())
        target = QPoint(round(origin.x()+dest.width()/2), round(origin.y() + (42 + index*58 + 5 if zone != "available" else 40)))
        QTest.mouseMove(view,source); QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
        for step in range(1,13):
            p = source + (target-source)*step/12
            QTest.mouseMove(view,p); QTest.qWait(15)
        require(root.property("dragId") == id, "pointer capture did not start " + id)
        require(root.property("hoverZone") == zone, "wrong target " + str(root.property("hoverZone")))
        QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,target); QTest.qWait(220)
        require(root.property("dragId") == "", "capture leaked")
    initial = config()
    require(not backend.calls,"opening must not write Windows")
    # Unsupported components cannot begin a drag or change the saved state.
    source = center(item("component_start"))
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(view,source+QPoint(40,20)); QTest.qWait(40)
    require(not root.property("dragId"), "unsupported component began dragging")
    QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,source+QPoint(40,20))
    require(config() == initial and not backend.calls and not backend.drafts, "unsupported move wrote state")
    drag("taskview","left",0)
    require(config() == initial and not backend.calls, "unsupported Task View position applied")
    drag("taskview","left",1)
    require(backend.calls == [("one",8)], "drop did not automatically apply exactly once")
    require(config()["left"] == ["widgets","taskview"] and backend.drafts["one"] == config(), "confirmed configuration not saved")
    applied = config()
    # Coalesce successive gap edits into one command; displayed layout stays confirmed.
    for gap in (12,16,24):
        next_config = dict(applied); next_config["gap"] = gap
        require(root.commit(next_config), "valid gap refused")
    require(config() == applied, "unconfirmed configuration displayed as applied")
    QTest.qWait(240)
    require(backend.calls[-1] == ("one",24) and len(backend.calls) == 2, "changes did not coalesce")
    applied = config()
    # A busy native owner resumes the queued edit only after completion.
    backend.busy = True
    calls_before = len(backend.calls)
    next_config = dict(applied); next_config["gap"] = 28
    root.commit(next_config); QTest.qWait(230)
    require(len(backend.calls) == calls_before, "request sent while native owner busy")
    backend.busy = False; backend.changed.emit(); QTest.qWait(230)
    require(len(backend.calls) == calls_before+1 and config()["gap"] == 28, "queued edit did not resume")
    applied = config()
    # Leaving the editor must cancel a deferred change before it reaches Windows.
    backend.busy = True
    calls_before = len(backend.calls)
    hidden_config = dict(applied); hidden_config["gap"] = 30
    require(root.commit(hidden_config), "deferred hidden-page fixture refused")
    root.setVisible(False)
    backend.busy = False; backend.changed.emit(); QTest.qWait(230)
    require(len(backend.calls) == calls_before and config() == applied, "hidden editor applied a deferred native request")
    root.setVisible(True); QTest.qWait(30)
    require(not root.property("queuedMonitor"), "hidden editor retained a target monitor")
    # Escape also cancels a queued request while the native owner is busy.
    backend.busy = True
    require(root.commit(hidden_config), "deferred Escape fixture refused")
    QTest.keyClick(view,Qt.Key_Escape)
    backend.busy = False; backend.changed.emit(); QTest.qWait(230)
    require(len(backend.calls) == calls_before and config() == applied, "Escape applied a deferred native request")
    for suspend, resume in ((lambda: root.setEnabled(False), lambda: root.setEnabled(True)),
                            (view.hide, view.show), (view.showMinimized, view.showNormal)):
        require(root.commit(hidden_config), "deferred inactive-editor fixture refused")
        suspend()
        require(not root.property("canSubmit") and not root.commit(hidden_config), "inactive editor accepted a native request")
        QTest.qWait(230)
        require(len(backend.calls) == calls_before and config() == applied, "inactive editor sent a delayed native request")
        resume(); QTest.qWait(30)
        require(not root.property("queuedMonitor"), "inactive editor retained a target monitor")
    backend.apply_ok = False
    next_config = dict(applied); next_config["gap"] = 32
    root.commit(next_config); QTest.qWait(230)
    require(config() == applied and backend.drafts["one"] == applied, "failed operation saved/displayed")
    backend.apply_ok = True
    # A deferred request must not apply to a panel after the user leaves it.
    backend.busy = True
    next_config = dict(applied); next_config["gap"] = 36
    require(root.commit(next_config), "queued monitor-switch fixture refused")
    calls_before = list(backend.calls)
    root.setProperty("monitorIndex", 1); QTest.qWait(50)
    backend.busy = False; backend.changed.emit(); QTest.qWait(240)
    require(backend.calls == calls_before and backend.active == applied,
            "monitor switch applied a deferred request to the previous panel")
    root.setProperty("monitorIndex",1); QTest.qWait(100)
    require(not root.property("nativeAvailable"),"secondary advertised native support")
    require(not root.moveComponent("taskview","left",1),"secondary move accepted")
    root.resetLayout()
    require(backend.active == applied and backend.drafts["one"] == applied,"secondary reset affected primary")
    root.setProperty("monitorIndex",0); QTest.qWait(100)
    require(config() == applied,"switch lost confirmed primary configuration")
    backend.reset_ok = False
    root.resetLayout()
    require(config() == applied and backend.drafts["one"] == applied,"failed reset lost state")
    backend.reset_ok = True
    root.resetLayout(); QTest.qWait(150)
    require(config() == initial and "one" not in backend.drafts,"restore did not clear selected saved configuration")
    # Escape and outside drop must leave native state untouched.
    before_calls = list(backend.calls)
    source = center(item("component_taskview"))
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(view,source+QPoint(35,15)); QTest.qWait(50)
    QTest.keyClick(view,Qt.Key_Escape)
    QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,source+QPoint(35,15))
    require(config() == initial and backend.calls == before_calls and not root.property("dragId"),"Escape applied a change or leaked capture")
    QTest.mouseMove(view,QPoint(880,5)); QTest.qWait(250)
    # Original Quick Settings is one native group: network and volume travel together.
    drag("clock","right",2)
    require(config()["right"]==["overflow","input","clock","network","volume","desktop"],"clock did not swap with native Quick Settings group")
    drag("network","right",2)
    require(config()["right"]==initial["right"],"grouped network/volume drag split the original native group")
    drag("input","right",0)
    require(config()["right"]==["input","overflow","network","volume","clock","desktop"],"input group could not pass original icon group")
    drag("overflow","right",4)
    require(config()["right"]==["input","network","volume","clock","overflow","desktop"],"icon group could not move to the last tray slot")
    drag("overflow","right",0)
    require(config()["right"]==initial["right"],"tray groups did not return to their original order")
    before_calls=list(backend.calls)
    drag("clock","left",0)
    require(config()["right"]==initial["right"] and backend.calls==before_calls,"unsupported cross-container tray move applied")
    backend.apply_ok=False
    drag("clock","right",2)
    require(config()["right"]==initial["right"],"refused tray order was displayed or saved")
    backend.apply_ok=True
    root.resetLayout();QTest.qWait(30)
    require(not warnings,"QML warnings: " + str(warnings))
    view.grabWindow().save(str(Path(__file__).resolve().parents[1]/"tmp/taskbar-configuration.png"))
    view.close()
    # Exercise the actual scroll container contract at the minimum window height.
    harness = Path(__file__).resolve().parents[1]/"tmp/configuration-scroll-test.qml"
    harness.write_text('''import QtQuick
import QtQuick.Controls
import "../src/presentation"
ScrollView {
    id: viewport
    property var system
    clip: true
    contentWidth: availableWidth
    TaskbarConfiguration { width: parent.width; system: viewport.system; scrollTarget: viewport.contentItem }
}''', encoding="utf-8")
    view = QQuickView()
    view.engine().warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
    view.setInitialProperties({"system":backend})
    view.setResizeMode(QQuickView.SizeRootObjectToView)
    view.setSource(QUrl.fromLocalFile(str(harness)))
    require(view.status() == QQuickView.Ready, str(view.errors()))
    view.resize(700,560); view.show(); QTest.qWait(150)
    root = view.rootObject()
    editor = item("taskbarConfiguration")
    source = center(item("component_taskview"))
    calls_before = list(backend.calls)
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(view,QPoint(100,540)); QTest.qWait(400)
    flickable = editor.property("scrollTarget")
    require(editor.property("dragId") == "taskview" and flickable.property("contentY") > 0,
            "captured drag failed to scroll at the viewport edge")
    QTest.keyClick(view,Qt.Key_Escape)
    QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,QPoint(100,540)); QTest.qWait(200)
    require(not editor.property("dragId") and backend.calls == calls_before,"scroll cancellation wrote native state")
    require(not warnings,"QML warnings: " + str(warnings))
    view.close()
    print("Passed: real Qt drag/drop, unsupported drag gating, automatic apply/save, coalescing, failure rollback, two-monitor isolation, selected restore, Escape, minimum-height edge scrolling; no QML warnings.")

if __name__ == "__main__": main()
