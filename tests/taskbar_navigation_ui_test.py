"""Real Settings.qml navigation/layout checks with a fake backend; no Windows writes."""
import os
from pathlib import Path
import subprocess
import sys
from xml.sax.saxutils import escape
import PySide6
from PySide6.QtCore import Property, QResource, QUrl, Qt
from PySide6.QtGui import QGuiApplication, QFontDatabase
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
from taskbar_configuration_ui_test import Backend, require

class NavigationBackend(Backend):
    available = Property(bool, lambda self: True, constant=True)
    widthAvailable = Property(bool, lambda self: True, constant=True)
    sidesAvailable = Property(bool, lambda self: True, constant=True)
    edge = Property(str, lambda self: "bottom", constant=True)
    detached = Property(bool, lambda self: False, constant=True)
    islands = Property(bool, lambda self: False, constant=True)
    autoHide = Property(bool, lambda self: False, constant=True)
    transparency = Property(int, lambda self: 35, constant=True)
    thickness = Property(int, lambda self: 48, constant=True)
    widthPercent = Property(int, lambda self: 100, constant=True)
    appearanceMonitors = Property("QVariantList", lambda self: [], constant=True)
    selectedAppearanceMonitor = Property(str, lambda self: "", constant=True)


def main():
    workspace = Path(__file__).resolve().parents[1]
    output = workspace / "tmp"
    output.mkdir(exist_ok=True)
    qrc = output / "navigation-test.qrc"
    resource = output / "navigation-test.rcc"
    qrc.write_text('<RCC><qresource prefix="/"><file alias="assets/umbrella.png">' + escape((workspace/"assets/umbrella.png").as_posix()) + '</file></qresource></RCC>', encoding="utf-8")
    subprocess.run([str(Path(PySide6.__file__).parent/"rcc.exe"), "--binary", str(qrc), "-o", str(resource)], check=True)
    desktop = "--desktop" in sys.argv
    app = QGuiApplication(["Umbrella navigation test"] + ([] if desktop else ["-platform", "offscreen"]))
    QFontDatabase.addApplicationFont("C:/Windows/Fonts/segoeui.ttf")
    QFontDatabase.addApplicationFont("C:/Windows/Fonts/segmdl2.ttf")
    require(QResource.registerResource(str(resource)), "logo resource registration failed")
    backend = NavigationBackend()
    view = QQuickView()
    warnings = []
    view.engine().warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
    view.rootContext().setContextProperty("taskbarSystem", backend)
    view.setSource(QUrl.fromLocalFile(str(workspace/"src/presentation/Settings.qml")))
    require(view.status() == QQuickView.Ready, str(view.errors()))
    root = view.rootObject()
    view.setResizeMode(QQuickView.SizeRootObjectToView)
    view.resize(1000,700)
    view.setTitle("Umbrella — проверка навигации")
    view.show()
    root.setProperty("firstFrameReady", True)
    QTest.qWait(4500)
    require(root.property("phase") == 3, "startup animation did not finish")
    def item(name):
        def walk(node):
            if node.objectName() == name: return node
            for child in node.childItems():
                found = walk(child)
                if found is not None: return found
        found = walk(root)
        require(found is not None, "missing " + name)
        return found
    def click(obj):
        point = obj.mapToScene(obj.boundingRect().center()).toPoint()
        QTest.mouseClick(view,Qt.LeftButton,Qt.NoModifier,point)
        QTest.qWait(260)
    click(item("taskbarNavigation"))
    require(root.property("taskbarExpanded") and not root.property("homeOpen"), "Taskbar did not expand")
    click(item("taskbarNavigation_configuration"))
    editor = item("taskbarConfiguration")
    require(root.property("taskbarSection") == "configuration" and editor.isVisible(), "configuration page not opened")
    require(not item("taskbarPage").isEnabled(), "appearance page remained interactive")
    click(item("configurationMonitor_1"))
    require(editor.property("monitorIndex") == 1 and editor.property("nativeAvailable"), "secondary availability incorrect")
    click(item("configurationMonitor_0"))
    require(editor.property("monitorIndex") == 0, "primary tab failed")
    click(item("taskbarNavigation_settings"))
    require(item("taskbarPage").isEnabled() and not editor.isVisible(), "settings page not restored")
    click(item("taskbarNavigation_configuration"))
    view.resize(800,560); QTest.qWait(260)
    scroll = item("configurationPageScroll")
    require(editor.width() > 400 and scroll.width() < view.width(), "minimum-size layout overflow")
    click(item("dockNavigation"))
    require(root.property("taskbarSection") == "dock" and not root.property("taskbarExpanded"), "Dock must be a top-level page")
    click(item("taskbarNavigation"))
    require(root.property("taskbarSection") == "settings" and root.property("taskbarExpanded"), "Taskbar navigation must leave Dock")
    require(not backend.calls, "navigation wrote Windows settings")
    require(not warnings, "QML warnings: " + str(warnings))
    view.grabWindow().save(str(output/"taskbar-navigation-800.png"))
    view.resize(1000,700); QTest.qWait(260)
    view.grabWindow().save(str(output/"taskbar-navigation-1000.png"))
    if desktop: QTest.qWait(45000)
    view.close()
    print("Passed: real Settings startup, expandable navigation, separate pages, monitor tabs, 800x560 layout; no QML warnings or Windows writes.")

if __name__ == "__main__": main()
