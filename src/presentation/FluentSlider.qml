import QtQuick
import QtQuick.Controls

Slider {
    id: control
    SettingsTheme { id: theme }
    implicitHeight: 36
    implicitWidth: 160
    hoverEnabled: true
    background: Rectangle {
        x: control.leftPadding; y: control.topPadding + (control.availableHeight-height)/2
        width: control.availableWidth; height: 4; radius: 2; color: theme.line
        Rectangle { width: control.visualPosition * parent.width; height: 4; radius: 2; color: theme.accent }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth-width)
        y: control.topPadding + (control.availableHeight-height)/2
        width: 22; height: 22; radius: 11
        color: theme.card; border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? theme.accent : theme.line
        Rectangle {
            anchors.centerIn: parent; width: control.pressed ? 10 : control.hovered ? 14 : 12
            height: width; radius: width/2; color: theme.accent
            Behavior on width { NumberAnimation { duration: theme.fast; easing.type: Easing.OutCubic } }
        }
    }
}
