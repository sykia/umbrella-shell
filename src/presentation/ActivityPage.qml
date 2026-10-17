import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    contentWidth: availableWidth
    property int offset: 0
    readonly property var prefs: shell.preferences
    readonly property var theme: prefs.theme
    readonly property var stats: { prefs.tracking; prefs.activityRevision; return prefs.activity(offset) }
    function duration(seconds) { return Math.floor(seconds / 3600) + qsTr("ч ") + Math.floor((seconds % 3600) / 60) + qsTr("м") }
    component Caption: Label { color: page.theme.ink; font.family: page.prefs.fontFamily; font.pixelSize: 11 }
    component Tile: Rectangle { radius: page.prefs.cornerRadius; color: page.theme.surface; Layout.fillWidth: true }
    Dialog { id: clearDialog; anchors.centerIn: parent; title: qsTr("Удалить всю историю?"); modal: true; standardButtons: Dialog.Ok | Dialog.Cancel; onAccepted: page.prefs.clearActivity() }
    ColumnLayout {
        width: page.availableWidth; spacing: 8
        RowLayout {
            UiButton { text: "‹"; enabled: page.offset > -89; onClicked: --page.offset }
            Caption { text: page.offset === 0 ? qsTr("Сегодня") : page.stats.date; font.bold: true; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
            UiButton { text: "›"; enabled: page.offset < 0; onClicked: ++page.offset }
        }
        RowLayout {
            Layout.fillWidth: true
            Tile { implicitHeight: 54; Caption { anchors.centerIn: parent; text: page.duration(page.stats.average) + "\n" + qsTr("В среднем за неделю"); horizontalAlignment: Text.AlignHCenter } }
            Tile { implicitHeight: 54; Caption { anchors.centerIn: parent; text: page.duration(page.stats.total); font.pixelSize: 22; font.bold: true } }
            Tile { implicitHeight: 54; Caption { anchors.centerIn: parent; text: (page.stats.difference > 0 ? "+" : "−") + page.duration(Math.abs(page.stats.difference)); color: page.stats.difference > 0 ? page.theme.accent : "#438d53" } }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Tile {
                implicitHeight: 120; Layout.preferredWidth: 270
                Row {
                    anchors.fill: parent; anchors.margins: 9; spacing: 5
                    Repeater {
                        model: page.stats.week
                        delegate: Item {
                            required property var modelData
                            width: (parent.width - 30) / 7; height: parent.height
                            Rectangle {
                                anchors.bottom: dayLabel.top; anchors.bottomMargin: 5; width: parent.width
                                height: Math.max(2, (parent.height - 22) * modelData.seconds / Math.max(1, ...page.stats.week.map(d => d.seconds)))
                                radius: 3; color: modelData.selected ? page.theme.accent : page.theme.muted
                                Behavior on height { NumberAnimation { duration: 200 } }
                            }
                            Caption { id: dayLabel; anchors.bottom: parent.bottom; text: modelData.date; font.pixelSize: 9 }
                        }
                    }
                }
            }
            Tile {
                implicitHeight: 120; Layout.preferredWidth: 150
                Column { anchors.centerIn: parent; spacing: 5
                    Caption { anchors.horizontalCenter: parent.horizontalCenter; text: page.stats.month; font.bold: true }
                    Grid { columns: 7; spacing: 3
                        Repeater {
                            model: page.stats.calendar
                            delegate: Rectangle {
                                required property var modelData
                                width: 12; height: 12; radius: 3
                                color: modelData.seconds > 0 ? page.theme.accent : page.theme.raised
                                opacity: modelData.day === 0 ? 0 : modelData.seconds > 0 ? 0.45 + Math.min(0.55, modelData.seconds / 28800) : 1
                                border.width: modelData.selected ? 1 : 0; border.color: page.theme.ink
                                MouseArea { anchors.fill: parent; enabled: modelData.offset <= 0 && modelData.offset >= -89; onClicked: page.offset = modelData.offset }
                            }
                        }
                    }
                }
            }
        }
        Repeater {
            model: page.stats.apps
            delegate: Tile {
                required property var modelData
                implicitHeight: 44
                Rectangle { x: 8; y: 8; width: 28; height: 28; radius: 8; color: page.theme.raised
                    Caption { anchors.centerIn: parent; text: modelData.name.charAt(0).toUpperCase(); font.bold: true }
                }
                Caption { x: 44; y: 5; text: modelData.name; font.bold: true }
                Caption { anchors.right: parent.right; anchors.rightMargin: 8; y: 6; text: page.duration(modelData.seconds) }
                Rectangle { x: 44; y: 27; width: parent.width - 54; height: 7; radius: 4; color: page.theme.raised
                    Rectangle { height: parent.height; radius: parent.radius; width: parent.width * modelData.seconds / Math.max(1, page.stats.total); color: page.theme.accent; Behavior on width { NumberAnimation { duration: 180 } } }
                }
            }
        }
        Caption { visible: page.stats.apps.length === 0; text: qsTr("Пока нет данных за этот день."); Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
        PillSwitch { Layout.fillWidth: true; text: qsTr("Учитывать активность локально"); checked: page.prefs.tracking; onClicked: page.prefs.tracking = checked }
        Caption { text: qsTr("Всё время активного окна, включая время без ввода. Блокировка и сон исключаются. Без заголовков окон; история до 90 дней только на этом компьютере."); wrapMode: Text.WordWrap; Layout.fillWidth: true }
        UiButton { text: qsTr("Очистить историю"); onClicked: clearDialog.open() }
        Caption { text: page.prefs.error; visible: text.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: page.theme.accent }
    }
}
