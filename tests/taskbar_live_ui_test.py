"""QML integration checks without modifying the Windows taskbar.
Run with Python + PySide6 6.8.3. No GUI window or Win32 adapter is created.
"""
import os
from pathlib import Path

os.environ["QT_QUICK_CONTROLS_STYLE"] = "Basic"
from PySide6.QtCore import QObject, Property, Signal, Slot, QUrl, QEventLoop, QTimer
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlEngine, QQmlComponent

class Model(QObject):
    changed = Signal()
    def __init__(self):
        super().__init__()
        self.busy = False
        self.calls = []
        self.module_available = False
        self.sides_available = False
        self.block_system = True
    def set_block_system(self, value):
        self.block_system = value
        self.changed.emit()
    blockSystemTaskbarSettings = Property(bool, lambda self: self.block_system, set_block_system, notify=changed)
    available = Property(bool, lambda self: True, constant=True)
    widthAvailable = Property(bool, lambda self: self.module_available, notify=changed)
    sidesAvailable = Property(bool, lambda self: self.sides_available, notify=changed)
    edge = Property(str, lambda self: "bottom", constant=True)
    detached = Property(bool, lambda self: False, constant=True)
    transparency = Property(int, lambda self: 35, constant=True)
    widthPercent = Property(int, lambda self: 100, constant=True)
    thickness = Property(int, lambda self: 48, constant=True)
    islands = Property(bool, lambda self: False, constant=True)
    autoHide = Property(bool, lambda self: False, constant=True)
    pending = Property(bool, lambda self: self.busy, notify=changed)
    status = Property(str, lambda self: "", notify=changed)
    @Slot(str, bool, int, bool, int, int, bool)
    def apply(self, *values):
        self.calls.append(values)
    @Slot()
    def resetAppearance(self):
        pass

class DockModel(QObject):
    changed = Signal()
    def __init__(self):
        super().__init__()
        self.active = False
    enabled = Property(bool, lambda self: self.active, notify=changed)
    @Slot(bool, result=bool)
    def setEnabled(self, value):
        self.active = value
        self.changed.emit()
        return True

def wait(milliseconds=260):
    loop = QEventLoop()
    QTimer.singleShot(milliseconds, loop.quit)
    loop.exec()

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def main():
    app = QGuiApplication(["taskbar-live-ui-test", "-platform", "offscreen"])
    model = Model()
    engine = QQmlEngine()
    warnings = []
    engine.warnings.connect(lambda items: warnings.extend(item.toString() for item in items))
    path = Path(__file__).resolve().parents[1] / "src/presentation/TaskbarPage.qml"
    component = QQmlComponent(engine, QUrl.fromLocalFile(str(path)))
    require(not component.isError(), str([e.toString() for e in component.errors()]))
    page = component.createWithInitialProperties({"system": model})
    require(page is not None, "Could not create TaskbarPage")
    wait()
    require(not model.calls, "Opening settings must not modify Windows")
    blocker = page.findChild(QObject, "blockSystemTaskbarSettings")
    require(blocker is not None and blocker.property("checked"), "System Settings guard must reflect saved preference")
    blocker.setProperty("checked", False)
    blocker.toggled.emit()
    require(not model.block_system and not model.calls, "Opt-out must reach preference without applying taskbar changes")

    for value in range(40, 75):
        page.setProperty("transparency", value)
        page.scheduleApply()
    wait()
    require(len(model.calls) == 1 and model.calls[-1][2] == 74,
            "Slider burst must apply only its final value")

    model.busy = True
    model.changed.emit()
    page.setProperty("transparency", 81)
    page.scheduleApply()
    wait()
    require(len(model.calls) == 1, "Pending native request must not be overwritten")
    page.setProperty("transparency", 92)
    page.scheduleApply()
    model.busy = False
    model.changed.emit()
    wait()
    require(len(model.calls) == 2 and model.calls[-1][2] == 92,
            "Latest change during native request must be applied after completion")

    for edge in ("left", "right"):
        page.setProperty("edge", edge)
        page.scheduleApply()
        wait()
    page.setProperty("edge", "bottom")
    page.setProperty("detached", True)
    page.setProperty("widthPercent", 75)
    page.scheduleApply()
    wait()
    require(len(model.calls) == 2, "Unsupported layouts must not reach Windows adapter")
    model.module_available = True
    model.changed.emit()
    for value in range(50, 81):
        page.setProperty("widthPercent", value)
        page.scheduleApply()
    wait()
    require(len(model.calls) == 3 and model.calls[-1][4] == 80,
            "An available native module must receive only the last requested width")
    model.sides_available = True
    model.changed.emit()
    for edge in ("left", "right"):
        page.setProperty("edge", edge)
        page.scheduleApply()
        wait()
        require(model.calls[-1][0] == edge, "Verified native side must reach the adapter")
    page.setProperty("edge", "bottom")
    page.setProperty("widthPercent", 75)
    toggle = page.findChild(QObject, "taskbarAutoHide")
    require(toggle is not None, "Autohide control must exist")
    toggle.setProperty("checked", True)
    toggle.toggled.emit()
    wait()
    require(page.property("widthPercent") == 100 and model.calls[-1][3] and model.calls[-1][4] == 100,
            "Enabling autohide must explicitly use the supported full-width layout")
    islands = page.findChild(QObject, "taskbarStyleIslands")
    require(islands is not None, "Grouped islands control must exist")
    islands.clicked.emit()
    wait()
    require(page.property("islands") and not page.property("autoHide") and page.property("thickness") >= 64,
            "Grouped islands require room for native content and explicitly disable incompatible autohide")
    require(model.calls[-1][6] and model.calls[-1][5] >= 64, "Native adapter must receive islands and thickness together")
    for value in range(64, 85, 2):
        page.setProperty("thickness", value)
        page.scheduleApply()
    wait()
    require(model.calls[-1][5] == 84, "Thickness slider must keep the latest size")
    width_section = page.findChild(QObject, "taskbarWidthSection")
    height_section = page.findChild(QObject, "taskbarThicknessSection")
    require(width_section is not None and height_section is not None, "Both independent size controls must exist")
    require(width_section.property("visible") and height_section.property("visible"), "Islands expose width and height")
    page.setProperty("widthPercent", 68)
    page.scheduleApply()
    wait()
    require(model.calls[-1][4] == 68 and model.calls[-1][1] and model.calls[-1][6], "Islands width must reach native layout")
    attached = page.findChild(QObject, "taskbarStyleAttached")
    separate = page.findChild(QObject, "taskbarStyleDetached")
    attached.clicked.emit()
    wait(350)
    require(page.property("panelStyle") == "attached" and page.property("thickness") == 48,
            "Attached style must use standard thickness")
    require(not width_section.property("visible") and not height_section.property("visible"),
            "Attached style hides custom width and height")
    separate.clicked.emit()
    wait(350)
    require(page.property("panelStyle") == "detached" and not page.property("islands"), "Separate style must exclude islands")
    require(width_section.property("visible") and height_section.property("visible"), "Separate style exposes both dimensions")
    page.setProperty("thickness", 60)
    page.scheduleApply()
    wait()
    islands.clicked.emit()
    wait()
    require(page.property("thickness") == 84 and page.property("widthPercent") == 68, "Returning to islands restores their size")
    separate.clicked.emit()
    wait()
    require(page.property("thickness") == 60, "Separate style retains its own height")
    for control in (attached, separate, islands):
        require(bool(control.property("selected")) == (control is separate), "Exactly one panel style must be selected")
    dock = DockModel()
    page.setProperty("dock", dock)
    previous = len(model.calls)
    page.selectStyle("dock")
    wait()
    require(dock.active and page.property("panelStyle") == "dock" and len(model.calls) == previous,
            "Enabling compatible Dock must preserve the native configuration")
    page.selectStyle("detached")
    wait()
    require(not dock.active and page.property("thickness") == 60 and len(model.calls) == previous,
            "Disabling Dock must restore the previous mode without an unnecessary native write")
    require(not warnings, "QML warnings: " + str(warnings))
    page.deleteLater()
    app.processEvents()
    print("Passed: no startup write, slider coalescing, pending latest value, unsupported guards; no QML warnings.")

if __name__ == "__main__":
    main()
