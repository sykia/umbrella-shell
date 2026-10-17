import QtQuick
import QtQuick.Controls
Grid {
    id: group
    required property int zone
    property bool vertical: false
    property bool logo: false
    readonly property var theme: shell.preferences.theme
    columns: vertical ? 1 : 4
    spacing: 6
    component PanelButton: Button {
        width: 26; height: 26
        font.family: shell.preferences.fontFamily; font.pixelSize: 11; font.bold: true
        icon.width: 15; icon.height: 15; icon.color: group.theme.ink
        palette.buttonText: group.theme.ink
        background: Rectangle {
            radius: 13; color: parent.down ? group.theme.accent : (parent.hovered ? Qt.tint(group.theme.surface, Qt.rgba(group.theme.accent.r, group.theme.accent.g, group.theme.accent.b, 0.22)) : group.theme.raised)
            Behavior on color { ColorAnimation { duration: 140 } }
        }
    }
    PanelButton {
        visible: group.logo
        padding: 0
        Accessible.name: qsTr("Показать или скрыть настройки Umbrella")
        contentItem: Item {
            Image {
                anchors.centerIn: parent; width: 30; height: 30
                source: "qrc:/assets/umbrella.png"; sourceSize: Qt.size(128, 128)
                fillMode: Image.PreserveAspectFit; mipmap: true; smooth: true
            }
        }
        onClicked: shell.toggleSettings()
    }
    Grid {
        id: spaces
        visible: shell.moduleZones.workspaces === group.zone
        columns: group.vertical ? 1 : shell.workspaceCount; spacing: 2
        property var items: { shell.moduleZones; return shell.workspaceModel(panelId) }
        Repeater {
            // Keep controls alive when occupancy or unrelated shell properties change.
            model: shell.workspaceCount
            delegate: Button {
                id: dot
                required property int index
                readonly property var modelData: spaces.items[index] || { index: index, active: false, occupied: false }
                enabled: spaces.items.length === shell.workspaceCount
                width: 30; height: 26
                Accessible.name: qsTr("Пространство ") + (modelData.index + 1) + (modelData.active ? qsTr(", активно") : "")
                background: Rectangle { color: "transparent" }
                contentItem: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: dot.modelData.active ? 28 : 12; height: 12; radius: 6
                        color: dot.modelData.active ? group.theme.accent : (dot.modelData.occupied ? "#aa8791" : "#d4ccd1")
                        scale: dot.hovered ? 1.17 : 1
                        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 170 } }
                        Behavior on scale { NumberAnimation { duration: 130 } }
                    }
                }
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
