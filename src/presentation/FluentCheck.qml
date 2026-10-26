import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    SettingsTheme { id: theme }
    implicitHeight: Math.max(40, implicitContentHeight + 16)
    leftPadding: 0; rightPadding: 8; topPadding: 8; bottomPadding: 8
    spacing: 12
    hoverEnabled: true
    font.family: "Segoe UI Variable"; font.pixelSize: 13
    indicator: Rectangle {
        x: 0; y: (control.height-height)/2; width: 20; height: 20; radius: 4
        color: control.checked ? theme.accent : control.hovered ? theme.hover : theme.card
        border.width: control.visualFocus ? 2 : 1
        border.color: control.checked || control.visualFocus ? theme.accent : theme.muted
        Behavior on color { ColorAnimation { duration: theme.fast } }
        Text { anchors.centerIn: parent; text: "\uE73E"; font.family: "Segoe Fluent Icons"; font.pixelSize: 12; color: theme.onAccent; visible: control.checked }
    }
    contentItem: Text {
        text: control.text; font: control.font; color: theme.ink
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter; wrapMode: Text.WordWrap
    }
}
