import QtQuick
import QtQuick.Controls
Switch {
    id: control
    readonly property var theme: shell.preferences.theme
    implicitWidth: Math.max(200, implicitContentWidth + 64)
    implicitHeight: 38
    leftPadding: 0; rightPadding: 0; spacing: 12
    font.family: shell.preferences.fontFamily; font.pixelSize: 12
    indicator: Rectangle {
        implicitWidth: 46; implicitHeight: 26
        x: control.width - width; y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? (control.down ? Qt.darker(control.theme.accent, 1.15) : control.theme.accent) : control.theme.raised
        border.width: control.activeFocus ? 2 : (control.hovered ? 1 : 0)
        border.color: control.theme.accent
        opacity: control.enabled ? 1 : 0.5
        Behavior on color { ColorAnimation { duration: 160 } }
        Rectangle {
            width: control.down ? 23 : 20; height: 20; radius: 10
            x: control.checked ? parent.width - width - 3 : 3
            y: 3; color: "white"
            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            Behavior on width { NumberAnimation { duration: 120 } }
        }
    }
    contentItem: Label {
        text: control.text; color: control.theme.ink
        font: control.font; verticalAlignment: Text.AlignVCenter
        rightPadding: 58; wrapMode: Text.WordWrap
        opacity: control.enabled ? 1 : 0.5
    }
}
