"""Compact islands retain explicitly selected thickness on component notifications."""
from pathlib import Path
from PySide6.QtCore import Property, Slot, QUrl
from PySide6.QtGui import QGuiApplication, QFontDatabase
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
from taskbar_navigation_ui_test import NavigationBackend
from taskbar_configuration_ui_test import require

class CompactBackend(NavigationBackend):
    mode = False
    size = 48
    detached = Property(bool, lambda self: self.mode, notify=NavigationBackend.changed)
    islands = Property(bool, lambda self: self.mode, notify=NavigationBackend.changed)
    thickness = Property(int, lambda self: self.size, notify=NavigationBackend.changed)
    @Slot(str, bool, int, bool, int, int, bool)
    def apply(self, edge, detached, alpha, hidden, width, thickness, islands):
        self.mode, self.size = islands, thickness
        self.calls.append(thickness)
        self.changed.emit()

def main():
    app=QGuiApplication(['compact islands', '-platform', 'offscreen'])
    QFontDatabase.addApplicationFont('C:/Windows/Fonts/segoeui.ttf')
    backend=CompactBackend();view=QQuickView();warnings=[]
    view.engine().warnings.connect(lambda values:warnings.extend(v.toString() for v in values))
    view.setInitialProperties({'system':backend})
    view.setSource(QUrl.fromLocalFile(str(Path(__file__).resolve().parents[1]/'src/presentation/TaskbarPage.qml')))
    require(view.status()==QQuickView.Ready,str(view.errors()))
    view.resize(900,1000);view.show();QTest.qWait(100);page=view.rootObject()
    page.selectStyle('islands');QTest.qWait(240)
    require(backend.calls==[48] and page.property('thickness')==48,'islands forced an oversized default')
    page.setProperty('thickness',56);page.scheduleApply();QTest.qWait(240)
    require(backend.size==56,'explicit thickness not applied')
    backend.changed.emit();QTest.qWait(240)
    require(page.property('thickness')==56 and backend.calls==[48,56],'component notification changed thickness or reapplied appearance')
    require(not warnings,str(warnings));view.close()
    print('Passed: compact 48 DIP islands, explicit 56 DIP retained on component notifications; no extra style apply.')

if __name__=='__main__': main()
