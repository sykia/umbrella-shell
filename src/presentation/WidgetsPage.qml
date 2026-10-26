import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    SettingsTheme { id: theme }
    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: page.availableWidth
        spacing: 16
        Text { text: "Widgets"; color: theme.ink; font.pixelSize: 30; font.weight: Font.DemiBold }
        Text { Layout.fillWidth: true; text: "Рабочее пространство на каждом экране"; wrapMode: Text.WordWrap; color: theme.ink; font.pixelSize: 18 }
        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Добавляйте часы на нужный монитор и настраивайте формат, стиль, цвет и интервалы."; color: theme.muted; font.pixelSize: 14 }
        Repeater {
            model: widgetSystem.monitors
            Rectangle {
                id: card
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 96
                radius: 12; color: theme.card; border.color: theme.line
                scale: configure.hovered ? 1.005 : 1
                Behavior on scale { NumberAnimation { duration: theme.fast; easing.type: Easing.OutCubic } }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 20; spacing: 14
                    Rectangle {
                        Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 11; color: theme.soft
                        Text { anchors.centerIn: parent; text: "\uE7F4"; font.family: "Segoe Fluent Icons"; font.pixelSize: 21; color: theme.ink }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Text { text: card.modelData.label + (card.modelData.primary ? " · Основной" : ""); color: theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Text {
                            readonly property int count: widgetSystem.instances.filter(item => item.screenId === card.modelData.id).length
                            text: card.modelData.width + " × " + card.modelData.height + " DIP · " + (count === 0 ? "часов нет" : "часы: " + count)
                            color: theme.muted; font.pixelSize: 12
                        }
                    }
                    FluentButton {
                        id: configure
                        objectName: "openWidgetEditor_" + card.modelData.id
                        text: "Открыть редактор"
                        Accessible.name: "Открыть редактор · " + card.modelData.label
                        onClicked: widgetSystem.openEditor(card.modelData.id)
                    }
                }
            }
        }
        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: widgetSystem.status; color: theme.muted; font.pixelSize: 13; visible: text.length > 0 }
    }
}
