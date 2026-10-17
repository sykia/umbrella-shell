import QtQuick
import QtQuick.Controls
Rectangle {
    color: "#191c23"
    Rectangle { width: parent.width; height: 1; color: "#363b47" }
    ToolButton {
        x: 8; anchors.verticalCenter: parent.verticalCenter
        width: 48; height: 48
        Accessible.name: qsTr("Настройки Umbrella Shell")
        ToolTip.visible: hovered; ToolTip.text: Accessible.name
        contentItem: Image { source: "qrc:/assets/umbrella.png"; fillMode: Image.PreserveAspectFit; sourceSize: Qt.size(48, 48); mipmap: true }
        onClicked: shell.openSettings()
    }
}
