import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool accent: false
    readonly property var theme: shell.preferences.theme
    implicitHeight: 32
    implicitWidth: Math.max(32, implicitContentWidth + 24)
    leftPadding: 12; rightPadding: 12; topPadding: 5; bottomPadding: 5
    font.family: shell.preferences.fontFamily; font.pixelSize: 11; font.bold: true
    scale: down ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: 100 } }
    contentItem: Label {
        text: control.text; font: control.font
        color: control.accent ? "white" : control.theme.ink
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideMiddle; opacity: control.enabled ? 1 : 0.45
    }
    background: Rectangle {
        radius: Math.max(8, Math.min(16, shell.preferences.cornerRadius))
        color: control.accent ? (control.down ? Qt.darker(control.theme.accent, 1.1) : control.theme.accent)
            : control.hovered ? Qt.tint(control.theme.raised, Qt.rgba(control.theme.accent.r, control.theme.accent.g, control.theme.accent.b, 0.15)) : control.theme.raised
        border.width: control.activeFocus || control.hovered ? 1 : 0
        border.color: control.theme.accent
        Behavior on color { ColorAnimation { duration: 150 } }
    }
}
