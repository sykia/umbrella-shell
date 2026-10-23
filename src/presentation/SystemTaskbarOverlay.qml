import QtQuick

Item {
    id: overlay
    property bool darkTheme: false
    property bool animationsEnabled: true
    property color accentColor: "#005fb8"
    readonly property bool running: Window.window && Window.window.visible && animationsEnabled
    // DWM supplies the live Desktop Acrylic. No capture, copied bitmap or shader loop.
    Rectangle { anchors.fill: parent; color: overlay.darkTheme ? "#32101010" : "#32ffffff" }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
    Column {
        anchors.centerIn: parent
        width: Math.max(0, Math.min(parent.width - 32, 500))
        spacing: 12
        Image {
            id: logo
            anchors.horizontalCenter: parent.horizontalCenter
            width: 76; height: 76
            source: "qrc:/assets/umbrella.png"
            fillMode: Image.PreserveAspectFit
            smooth: true; mipmap: true
            RotationAnimator on rotation {
                from: 0; to: 360; duration: 10000; loops: Animation.Infinite
                running: overlay.running
            }
        }
        Text {
            width: parent.width
            text: "Панель задач управляется Umbrella"
            font.family: "Segoe UI Variable"; font.pixelSize: 20; font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            color: overlay.accentColor
        }
        Text {
            width: parent.width
            text: "Настройки этой категории недоступны, пока используется Umbrella Shell."
            font.family: "Segoe UI Variable"; font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            color: overlay.darkTheme ? "#dddddd" : "#404040"
        }
    }
}
