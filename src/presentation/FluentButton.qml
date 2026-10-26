import QtQuick
import QtQuick.Controls

Button {
    id: control
    SettingsTheme { id: theme }
    hoverEnabled: true
    implicitHeight: 34
    implicitWidth: Math.max(96, implicitContentWidth + 28)
    leftPadding: 14; rightPadding: 14; topPadding: 7; bottomPadding: 7
    font.family: "Segoe UI Variable"
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45
    scale: down ? 0.98 : 1
    Behavior on scale { NumberAnimation { duration: theme.fast; easing.type: Easing.OutCubic } }
    background: Rectangle {
        radius: 5
        color: control.down ? theme.soft : control.hovered ? theme.hover : theme.card
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? theme.accent : theme.line
        Behavior on color { ColorAnimation { duration: theme.fast } }
    }
    contentItem: Text {
        text: control.text; font: control.font; color: theme.ink
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
