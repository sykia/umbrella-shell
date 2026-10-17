import QtQuick
import QtQuick.Controls
TextField {
    id: control
    implicitHeight: 32; leftPadding: 12; rightPadding: 12
    color: shell.preferences.theme.ink
    placeholderTextColor: shell.preferences.theme.muted
    font.family: shell.preferences.fontFamily; font.pixelSize: 11
    selectionColor: shell.preferences.theme.accent
    background: Rectangle {
        radius: Math.max(8, Math.min(16, shell.preferences.cornerRadius))
        color: shell.preferences.theme.raised
        border.width: control.activeFocus ? 1 : 0
        border.color: shell.preferences.theme.accent
        Behavior on border.color { ColorAnimation { duration: 140 } }
    }
}
