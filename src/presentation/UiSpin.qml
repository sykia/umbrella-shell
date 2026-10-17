import QtQuick
import QtQuick.Controls
SpinBox {
    id: control
    implicitHeight: 32; implicitWidth: 110; leftPadding: 32; rightPadding: 32
    contentItem: Label { text: control.textFromValue(control.value, control.locale); color: shell.preferences.theme.ink; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.family: shell.preferences.fontFamily; font.pixelSize: 11; font.bold: true }
    background: Rectangle { radius: 16; color: shell.preferences.theme.raised; border.width: control.activeFocus ? 1 : 0; border.color: shell.preferences.theme.accent }
    down.indicator: Rectangle {
        x: 3; y: 3; width: 26; height: control.height - 6; radius: 13
        color: control.down.pressed ? shell.preferences.theme.accent : shell.preferences.theme.surface
        Label { anchors.centerIn: parent; text: "−"; color: control.down.pressed ? "white" : shell.preferences.theme.ink; font.pixelSize: 17 }
        Behavior on color { ColorAnimation { duration: 130 } }
    }
    up.indicator: Rectangle {
        x: control.width - width - 3; y: 3; width: 26; height: control.height - 6; radius: 13
        color: control.up.pressed ? shell.preferences.theme.accent : shell.preferences.theme.surface
        Label { anchors.centerIn: parent; text: "+"; color: control.up.pressed ? "white" : shell.preferences.theme.ink; font.pixelSize: 17 }
        Behavior on color { ColorAnimation { duration: 130 } }
    }
}
