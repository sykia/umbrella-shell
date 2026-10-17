import QtQuick
import QtQuick.Controls
Grid {
    id: group
    required property int zone
    property bool vertical: false
    property bool logo: false
    columns: vertical ? 1 : 4
    spacing: 6
    component PanelButton: Button {
        width: 26; height: 26
        font.family: "Segoe UI"; font.pixelSize: 11; font.bold: true
        icon.width: 15; icon.height: 15; icon.color: "#585365"
        palette.buttonText: "#585365"
        background: Rectangle {
            radius: 13; color: parent.down ? "#d9263b" : (parent.hovered ? "#efcbd2" : "#dfe1e9")
            Behavior on color { ColorAnimation { duration: 140 } }
        }
    }
    PanelButton {
        visible: group.logo
        Accessible.name: qsTr("Показать или скрыть настройки Umbrella")
        contentItem: Image { source: "qrc:/assets/umbrella.png"; sourceSize: Qt.size(32, 32); fillMode: Image.PreserveAspectFit; mipmap: true }
        onClicked: shell.toggleSettings()
    }
    Grid {
        visible: shell.moduleZones.workspaces === group.zone
        columns: group.vertical ? 1 : 5; spacing: 2
        property var items: { shell.moduleZones; return shell.workspaceModel(panelId) }
        Repeater {
            model: parent.items
            delegate: Button {
                id: dot
                required property var modelData
                width: modelData.active ? 28 : 20; height: 26
                Accessible.name: qsTr("Пространство ") + (modelData.index + 1) + (modelData.active ? qsTr(", активно") : "")
                background: Rectangle { color: "transparent" }
                contentItem: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: dot.modelData.active ? 23 : 9; height: 9; radius: 5
                        color: dot.modelData.active ? "#d9263b" : (dot.modelData.occupied ? "#b96c7a" : "#d4ccd1")
                        scale: dot.hovered ? 1.17 : 1
                        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 170 } }
                        Behavior on scale { NumberAnimation { duration: 130 } }
                    }
                }
                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                onClicked: shell.selectWorkspace(panelId, modelData.index)
            }
        }
    }
    PanelButton {
        visible: shell.moduleZones.keyboard === group.zone
        width: group.vertical ? 26 : 58
        text: shell.keyboardLayout
        icon.source: group.vertical ? "" : "qrc:/assets/icons/keyboard.svg"
        leftPadding: 6; rightPadding: 6; spacing: 4
        Accessible.name: qsTr("Сменить раскладку клавиатуры: ") + text
        onClicked: shell.cycleKeyboard()
    }
    PanelButton {
        visible: shell.moduleZones.volume === group.zone
        width: group.vertical ? 26 : 68
        text: group.vertical ? "" : qsTr("Звук")
        leftPadding: 6; rightPadding: 6; spacing: 4
        icon.source: "qrc:/assets/icons/volume.svg"
        Accessible.name: qsTr("Параметры громкости Windows");
        onClicked: shell.openSound()
    }
}
