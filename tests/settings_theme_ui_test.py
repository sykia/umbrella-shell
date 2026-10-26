"""Settings palette changes with backend notifications; no Windows settings writes."""
from pathlib import Path
import sys
from PySide6.QtCore import QObject, Property, Signal, QUrl, QResource
from PySide6.QtGui import QGuiApplication, QColor, QFontDatabase
from PySide6.QtQuick import QQuickView
from PySide6.QtTest import QTest
from taskbar_navigation_ui_test import NavigationBackend
from taskbar_configuration_ui_test import require

class ThemeBackend(QObject):
    themeChanged = Signal()
    value = {}
    theme = Property('QVariantMap', lambda self: self.value, notify=themeChanged)
    def change(self, dark, accent, contrast=False, animations=True, transparency=False):
        self.value = dict(dark=dark, accent=QColor(accent), contrast=contrast,
                          animations=animations, transparency=transparency,
                          foreground=QColor('white' if dark else 'black'),
                          background=QColor('black' if dark else 'white'))
        self.themeChanged.emit()

def main():
    workspace = Path(__file__).resolve().parents[1]
    app = QGuiApplication(['theme test', '-platform', 'offscreen'])
    require(QResource.registerResource(str(workspace/'tmp/navigation-test.rcc')), 'run navigation UI test first')
    QFontDatabase.addApplicationFont('C:/Windows/Fonts/segoeui.ttf')
    QFontDatabase.addApplicationFont('C:/Windows/Fonts/segmdl2.ttf')
    app.setFont(QFontDatabase.font('Segoe UI', '', 10))
    backend, windows = NavigationBackend(), ThemeBackend()
    windows.change(False, '#0078d4')
    view = QQuickView()
    warnings = []
    view.engine().warnings.connect(lambda values: warnings.extend(w.toString() for w in values))
    view.rootContext().setContextProperty('taskbarSystem', backend)
    view.rootContext().setContextProperty('systemTheme', windows)
    view.setSource(QUrl.fromLocalFile(str(workspace/'src/presentation/Settings.qml')))
    require(view.status() == QQuickView.Ready, str(view.errors()))
    view.setResizeMode(QQuickView.SizeRootObjectToView)
    view.resize(1000,700)
    view.show()
    root = view.rootObject()
    root.setProperty('firstFrameReady',True)
    QTest.qWait(4300)
    def find(node, name):
        if node.objectName() == name: return node
        for child in node.childItems():
            result = find(child, name)
            if result is not None: return result
    page = find(root,'taskbarPage')
    for label, dark, accent, contrast in [('light',False,'#0078d4',False),('dark',True,'#ffb900',False),('contrast',True,'#ffff00',True)]:
        windows.change(dark,accent,contrast)
        QTest.qWait(80)
        require(page.property('accent').name() == accent, 'accent did not follow theme')
        require(root.property('color').lightness() < 128 if dark else root.property('color').lightness() > 128, 'background did not follow theme')
        view.grabWindow().save(str(workspace/f'tmp/settings-home-{label}.png'))
    theme = root.findChild(QObject, 'settingsTheme')
    require(theme is not None, 'missing settings theme')
    root.setProperty('backdropAvailable', True)
    windows.change(False, '#0078d4', animations=False, transparency=True)
    QTest.qWait(30)
    require(root.property('color').alpha() < 255, 'native material was covered by an opaque surface')
    require(theme.property('normal') == 0, 'Windows reduced motion was ignored')
    windows.change(False, '#0078d4', transparency=False)
    QTest.qWait(30)
    require(root.property('color').alpha() == 255, 'disabled transparency must be opaque')
    windows.change(True, '#ffff00', contrast=True, transparency=True)
    QTest.qWait(30)
    require(root.property('color').alpha() == 255 and theme.property('normal') == 0,
            'high contrast must disable transparency and motion')
    root.setProperty('backdropAvailable', False)
    windows.change(False, '#0078d4', transparency=True)
    QTest.qWait(30)
    require(root.property('color').alpha() == 255, 'failed native backdrop must be opaque')
    # A second window begins with reduced motion before its first frame.
    # Startup must finish without a zero-duration infinite waiting animation.
    windows.change(False, '#0078d4', animations=False)
    reduced = QQuickView()
    reduced.rootContext().setContextProperty('taskbarSystem', backend)
    reduced.rootContext().setContextProperty('systemTheme', windows)
    reduced.setSource(QUrl.fromLocalFile(str(workspace/'src/presentation/Settings.qml')))
    require(reduced.status() == QQuickView.Ready, str(reduced.errors()))
    reduced.show()
    reduced.rootObject().setProperty('firstFrameReady', True)
    QTest.qWait(100)
    require(reduced.rootObject().property('phase') == 3, 'reduced-motion startup did not settle')
    reduced.close()
    require(not warnings, str(warnings))
    require(not backend.calls, 'theme changes applied taskbar settings')
    view.close()
    print('Passed: light/dark/accent/high-contrast, reduced motion and material fallbacks; no QML warnings or taskbar writes.')

if __name__ == '__main__': main()
