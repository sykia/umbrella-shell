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
    layoutMonitors = Property("QVariantList", lambda self: [
        {"id":"one","label":"Основной","primary":True,"width":2560},
        {"id":"two","label":"Второй","primary":False,"width":2560}], constant=True)
    componentPlacementAvailable = Property(bool, lambda self: True, constant=True)
    status = Property(str, lambda self: "", constant=True)
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
    drag("trayapps","right",2)
    require(config()["right"][2] == "trayapps", "catalog-to-zone insertion failed")
    drag("taskview","left",0)
    require(config()["left"] == ["taskview","widgets"],"cross-zone insert before first failed")
    drag("widgets","center",1)
    require(config()["center"] == ["start","widgets","search","apps"],"insert between arbitrary items failed")
    drag("apps","center",0)
    require(config()["center"] == ["apps","start","widgets","search"],"same-zone upward reorder failed")
    drag("apps","center",3)
    require(config()["center"] == ["start","widgets","search","apps"],"same-zone downward reorder failed")
    drag("taskview","available")
    require("taskview" in config()["available"] and "taskview" not in config()["left"],"return to catalog failed")
    # Empty-zone target, using a catalog item now visible at the beginning of the strip.
    drag("widgets","left",0)
    require(config()["left"] == ["widgets"],"empty-zone insert failed")
    edited = config()
    require(not backend.calls,"editing must only modify the draft")
    source = center(item("component_start"))
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(view,source+QPoint(35,15)); QTest.qWait(50)
    QTest.keyClick(view,Qt.Key_Escape); QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,source+QPoint(35,15))
    require(config() == edited and root.property("dragId") == "", "Escape changed draft or leaked capture")
    QTest.mousePress(view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(view,QPoint(-20,10)); QTest.mouseRelease(view,Qt.LeftButton,Qt.NoModifier,QPoint(-20,10))
    require(config() == edited,"invalid drop changed draft")
    root.setProperty("monitorIndex",1); QTest.qWait(100)
    require(config() != edited and "overflow" in config()["available"],"secondary defaults were not independent")
    require(not root.property("nativeAvailable"),"secondary must not claim native support")
    drag("start","right",0)
    root.resetLayout()
    require(not backend.calls,"secondary reset must not reset the primary native session")
    root.setProperty("monitorIndex",0); QTest.qWait(100)
    require(config() == edited,"monitor switch lost primary draft")
    require(not root.moveComponent("unknown","left",0),"unknown ID accepted")
    require(not root.moveComponent("start","bad",0),"invalid zone accepted")
    require(not root.moveComponent("start","left",999),"invalid index accepted")
    backend.reset_ok = False
    root.resetLayout(); require(config() == edited,"failed native reset discarded draft")
    backend.reset_ok = True
    root.resetLayout(); QTest.qWait(150)
    require(config() == initial,"reset did not restore catalog baseline")
    backend.calls.clear()
    drag("taskview","left",1)
    require(root.property("applyScenario") == "after-widgets","supported scenario not exposed")
    QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,center(item("configurationApply"))); QTest.qWait(50)
    require(backend.calls == [("one",8)],"explicit Apply did not reach the monitor-specific adapter")
    drag("taskview","right",0)
    require(root.property("applyScenario") == "unsupported" and not item("configurationApply").isEnabled(),"unsupported scenario advertised as applicable")
    root.resetLayout(); QTest.qWait(150)
    QTest.mouseMove(view,QPoint(880,5)); QTest.qWait(150)
    require(not warnings,"QML warnings: " + str(warnings))
    view.grabWindow().save(str(Path(__file__).resolve().parents[1]/"tmp/taskbar-configuration.png"))
    view.close()
    # The real settings page is scrollable at its minimum height: a captured drag
    # must scroll to a zone's final insertion slot without leaving the editor.
    from taskbar_live_ui_test import Model
    full_backend = Model()
    full_view = QQuickView()
    full_view.engine().warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
    full_view.setInitialProperties({"system":full_backend})
    full_view.setResizeMode(QQuickView.SizeRootObjectToView)
    full_view.resize(900,560)
    full_view.setSource(QUrl.fromLocalFile(str(Path(__file__).resolve().parents[1]/"src/presentation/TaskbarPage.qml")))
    full_view.show(); QTest.qWait(150)
    root = full_view.rootObject()
    require(root is not None,"Taskbar page did not load")
    editor = item("taskbarConfiguration")
    source = center(item("component_taskview"))
    require(0 < source.y() < 560,"source is outside the initial viewport")
    destination = item("configuration_right")
    QTest.mousePress(full_view,Qt.LeftButton,Qt.NoModifier,source)
    QTest.mouseMove(full_view,QPoint(round(center(destination).x()),535))
    reached = False
    for _ in range(35):
        QTest.qWait(60)
        p = destination.mapToScene(destination.boundingRect().bottomRight())
        if p.y() < 520:
            target = QPoint(round(center(destination).x()),round(p.y()-8))
            QTest.mouseMove(full_view,target); QTest.qWait(30)
            QTest.mouseRelease(full_view,Qt.LeftButton,Qt.NoModifier,target)
            reached = True; break
    require(reached,"edge auto-scroll did not reveal the final insertion slot")
    QTest.qWait(180)
    require(editor.property("configuration").toVariant()["right"][-1] == "taskview","scrolling lost pointer capture or insertion order")
    require(not full_backend.calls,"scrolling editor changed Windows settings")
    require(not warnings,"QML warnings: " + str(warnings))
    full_view.close()
    print("Passed: actual Qt mouse drags, cross-zone insertion, both reorder directions, empty zone, catalog return, independent monitors, validation, reset refusal; no QML warnings or automatic Windows writes.")

if __name__ == "__main__": main()
