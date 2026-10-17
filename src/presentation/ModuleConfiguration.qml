import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: config
    required property var modules
    required property Item dragSurface
    spacing: 8
    Label { text: qsTr("Конфигурация панели"); font.bold: true; color: "#4c4145"; font.pixelSize: 12 }
    Label { text: qsTr("Зажмите модуль и перетащите в нужную зону. Отпускание за её пределами отменяет перенос."); Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#87727c"; font.pixelSize: 10 }
    component Zone: Rectangle {
        id: zoneCard
        required property int zone
        required property string title
        Layout.fillWidth: true
        radius: 12; color: drop.containsDrag ? "#f6dce1" : "#f8f4f5"
        border.width: drop.containsDrag ? 2 : 1
        border.color: drop.containsDrag ? "#d9263b" : "#e3d7dc"
        Behavior on color { ColorAnimation { duration: 150 } }
        Label { x: 9; y: 7; text: zoneCard.title; font.pixelSize: 10; font.bold: true; color: "#82717a" }
        DropArea {
            id: drop; anchors.fill: parent; keys: ["umbrella-module"]
            onDropped: event => {
                if (event.source && event.source.moduleKey) {
                    shell.setModuleZone(event.source.moduleKey, zoneCard.zone)
                    event.acceptProposedAction()
                }
            }
        }
        Flow {
            x: 7; y: 28; width: parent.width - 14; spacing: 6
            Repeater {
                model: config.modules
                delegate: Item {
                    id: holder
                    required property var modelData
                    visible: shell.moduleZones[modelData.key] === zoneCard.zone
                    width: zoneCard.zone === -1 ? Math.min(148, (zoneCard.width - 28) / 3) : zoneCard.width - 14
                    height: 34
                    Rectangle {
                        id: tile
                        property string moduleKey: holder.modelData.key
                        width: holder.width; height: holder.height; radius: 10
                        color: mouse.drag.active ? "#f2bdc7" : (mouse.containsMouse ? "#ead4db" : "#e5dce0")
                        border.color: mouse.drag.active ? "#d9263b" : "transparent"
                        scale: mouse.drag.active ? 1.04 : 1
                        Behavior on color { ColorAnimation { duration: 130 } }
                        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
                        Drag.active: mouse.drag.active
                        Drag.keys: ["umbrella-module"]
                        Drag.source: tile
                        Drag.hotSpot.x: width / 2; Drag.hotSpot.y: height / 2
                        Image { x: 8; anchors.verticalCenter: parent.verticalCenter; width: 14; height: 14; source: "qrc:/assets/icons/" + holder.modelData.icon + ".svg"; sourceSize: Qt.size(20, 20) }
                        Label { x: 27; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 32; text: holder.modelData.title; font.pixelSize: 10; font.bold: true; color: "#594650"; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                        Accessible.role: Accessible.Button
                        Accessible.name: holder.modelData.title
                        MouseArea {
                            id: mouse; anchors.fill: parent; hoverEnabled: true
                            drag.target: tile; drag.smoothed: false
                            onPressed: {
                                const point = tile.mapToItem(config.dragSurface, 0, 0)
                                tile.parent = config.dragSurface; tile.x = point.x; tile.y = point.y
                            }
                            onReleased: {
                                tile.Drag.drop()
                                tile.parent = holder; tile.x = 0; tile.y = 0
                            }
                            onCanceled: { tile.parent = holder; tile.x = 0; tile.y = 0 }
                        }
                    }
                }
            }
        }
    }
    Zone { zone: -1; title: qsTr("Доступные"); Layout.preferredHeight: 75 }
    RowLayout {
        Layout.fillWidth: true; spacing: 7
        Zone { zone: 0; title: qsTr("Слева"); Layout.preferredHeight: 154 }
        Zone { zone: 1; title: qsTr("По центру"); Layout.preferredHeight: 154 }
        Zone { zone: 2; title: qsTr("Справа"); Layout.preferredHeight: 154 }
    }
}
