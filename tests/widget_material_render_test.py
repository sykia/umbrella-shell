"""GPU material geometry/regression probe; fake wallpaper, no Windows writes.
Run with PySide6 6.8 and path to a binary RCC built from the GUI settings.qrc.
"""
import os
os.environ.setdefault('QSG_RENDER_LOOP', 'basic') # PySide callbacks hold the GIL.
import sys
import tempfile
from pathlib import Path
from PySide6.QtCore import QObject, Property, Signal, QUrl, QResource, QEventLoop, QTimer
from PySide6.QtGui import QGuiApplication, QColor, QImage, QPainter, QLinearGradient
from PySide6.QtQuick import QQuickView, QQuickImageProvider

class Theme(QObject):
    changed = Signal()
    values = dict(dark=False, accent=QColor('#4499dd'), contrast=False,
                  animations=False, transparency=True, backdropRevision=0)
    theme = Property('QVariantMap', lambda s: s.values, notify=changed)

class Backdrop(QQuickImageProvider):
    def __init__(self): super().__init__(QQuickImageProvider.Image)
    def requestImage(self, id, size, requested):
        image = QImage(768, 768, QImage.Format_ARGB32_Premultiplied)
        painter = QPainter(image)
        gradient = QLinearGradient(0, 0, 768, 0)
        gradient.setColorAt(0, QColor('#153f77'))
        gradient.setColorAt(1, QColor('#ffb050') if id.endswith('/0') else QColor('#44ff80'))
        painter.fillRect(image.rect(), gradient)
        painter.end()
        return image

def settle(milliseconds):
    loop = QEventLoop()
    QTimer.singleShot(milliseconds, loop.quit)
    loop.exec()

def main():
    root = Path(__file__).resolve().parents[1]
    app = QGuiApplication([])
    assert QResource.registerResource(sys.argv[1]), 'binary settings RCC required'
    theme = Theme()
    view = QQuickView()
    view.engine().addImageProvider('widgetBackdrop', Backdrop())
    warnings = []
    view.engine().warnings.connect(lambda w: warnings.extend(x.toString() for x in w))
    view.rootContext().setContextProperty('systemTheme', theme)
    qml = '''import QtQuick
import "qrc:/qt/qml/Umbrella" as UI
Rectangle {
    width: 800; height: 300; color: "#435640"
    property string chosen: "mica"
    property real offset: 20
    property bool nativeMode: false
    Item {
        id: owner; x: parent.offset; y: 20; width: 360; height: 240
        UI.MaterialText {
            objectName: "glyph"; anchors.fill: parent
            text: "12:48"; color: "white"; font.pixelSize: 100
            verticalAlignment: Text.AlignVCenter; materialStyle: chosen
            nativeAvailable: nativeMode
            materialOwner: owner
            materialPlacement: ({ x: 0, y: 0, screenWidth: 800, screenHeight: 600 })
        }
    }
    UI.WidgetSurface {
        x: 410; y: 20; width: 360; height: 240; materialStyle: chosen
        materialPlacement: ({x:400,y:0,screenWidth:800,screenHeight:600})
    }
}'''
    with tempfile.TemporaryDirectory() as folder:
        qml_file = Path(folder)/'probe.qml'
        qml_file.write_text(qml, encoding='utf-8')
        view.setSource(QUrl.fromLocalFile(str(qml_file)))
        assert view.status() == QQuickView.Ready, view.errors()
        view.show()
        settle(1600)
        images = {}
        for style in ['acrylic','mica','solid','transparent']:
            view.rootObject().setProperty('chosen',style)
            settle(200)
            image = view.grabWindow()
            assert not image.isNull()
            assert image.pixelColor(25,25) == QColor('#435640'), 'clock must have no card'
            images[style] = image
        assert images['transparent'].pixelColor(450,40) == QColor('#435640')
        if os.getenv('QT_QUICK_BACKEND') != 'software':
            assert images['mica'] != images['acrylic']
            view.rootObject().setProperty('chosen','mica')
            settle(200)
            before = view.grabWindow().copy(20,20,360,240)
            view.rootObject().setProperty('offset',40)
            settle(200)
            after = view.grabWindow().copy(40,20,360,240)
            assert before != after, 'glyph backdrop must follow monitor position'
            before = view.grabWindow()
            theme.values = dict(theme.values, backdropRevision=1)
            theme.changed.emit()
            settle(1600)
            assert before != view.grabWindow(), 'background notification must refresh texture'
            view.rootObject().setProperty('nativeMode',True)
            settle(200)
            native = view.grabWindow().copy(40,20,360,240)
            theme.values = dict(theme.values, backdropRevision=2)
            theme.changed.emit()
            settle(400)
            assert native == view.grabWindow().copy(40,20,360,240), 'native glyph mode must not paint cached wallpaper'
        theme.values = dict(theme.values, contrast=True, transparency=False,
                            foreground=QColor('white'), background=QColor('black'))
        theme.changed.emit()
        settle(200)
        assert not view.rootObject().findChild(QObject,'glyph').property('effectActive')
        assert not warnings, warnings
    print('PASS: glyph-only mask, transparent surface, coordinate updates, background invalidation, contrast/software fallback')

if __name__ == '__main__': main()
