import QtQuick
import QtQuick.Window
Item {
    id: root
    property bool animated: true
    implicitWidth: 124; implicitHeight: 124
    HoverHandler { id: hover }
    Image {
        anchors.fill: parent
        source: "qrc:/assets/umbrella.png"
        sourceSize: Qt.size(192, 192)
        fillMode: Image.PreserveAspectFit; mipmap: true
        scale: hover.hovered ? 1.08 : 1
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        transform: [
            Rotation { id: tilt; origin.x: root.width / 2; origin.y: root.height / 2; axis.x: 0; axis.y: 1; axis.z: 0 },
            Rotation { id: spin; origin.x: root.width / 2; origin.y: root.height / 2 }
        ]
    }
    FrameAnimation {
        running: root.animated && root.visible && root.Window.window !== null && root.Window.window.visible
        onTriggered: {
            spin.angle += frameTime * (hover.hovered ? 210 : 34) * Math.sin(elapsedTime * 0.45 + 0.8)
            tilt.angle = Math.sin(elapsedTime * 0.75) * 22
        }
    }
}
