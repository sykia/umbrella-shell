import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var record
    readonly property var options: record.settings || ({})
    SettingsTheme { id: theme }
    color: theme.card
    radius: 18
    border.color: theme.line

    function update(key, value) {
        const next = Object.assign({}, options)
        next[key] = value
        widgetSystem.updateClock(next)
    }
    function updateStyle(value) {
        const next = Object.assign({}, options)
        next.style = value
        if (value === "empty" && !next.color) next.color = "#FFFFFF"
        widgetSystem.updateClock(next)
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 18
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; text: "Настройка часов"; color: theme.ink; font.pixelSize: 18; font.weight: Font.DemiBold }
                FluentButton { text: "×"; implicitWidth: 34; leftPadding: 0; rightPadding: 0; onClicked: widgetSystem.removeInstance(panel.record.instanceId) }
            }
            Text { text: "Формат"; color: theme.muted; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                FluentButton { Layout.fillWidth: true; text: "00:00"; checkable: true; checked: panel.options.mode === "inline"; onClicked: panel.update("mode", "inline") }
                FluentButton { Layout.fillWidth: true; text: "00\n00"; checkable: true; checked: panel.options.mode === "stacked"; onClicked: panel.update("mode", "stacked") }
            }
            Text { text: "Стиль"; color: theme.muted; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                FluentButton { Layout.fillWidth: true; text: "Пустой"; checkable: true; checked: panel.options.style === "empty"; onClicked: panel.updateStyle("empty") }
                FluentButton { Layout.fillWidth: true; text: "Блюр фона"; checkable: true; checked: panel.options.style === "blur-card"; onClicked: panel.updateStyle("blur-card") }
                FluentButton { Layout.fillWidth: true; text: "Блюр цифр"; checkable: true; checked: panel.options.style === "blur-digits"; onClicked: panel.updateStyle("blur-digits") }
            }
            Text { text: "Цвет цифр"; color: theme.muted; font.pixelSize: 12 }
            GridLayout {
                Layout.fillWidth: true
                columns: 6
                rowSpacing: 10; columnSpacing: 10
                Repeater {
                    model: ["#FFF7F2", "#FFFFFF", "#151515", "#FFAA96", "#FF7D73", "#FFD36C", "#83D7C5", "#85BFFF", "#AFA0FF", "#ECA5D7", "#B7E47B", "#B8C1CC"]
                    Rectangle {
                        required property string modelData
                        Layout.preferredWidth: 30; Layout.preferredHeight: 30
                        radius: 15; color: modelData
                        border.width: panel.options.color === modelData ? 3 : 1
                        border.color: panel.options.color === modelData ? theme.accent : theme.line
                        scale: colorHover.hovered ? 1.10 : 1
                        Behavior on scale { NumberAnimation { duration: theme.fast; easing.type: Easing.OutCubic } }
                        HoverHandler { id: colorHover }
                        TapHandler { onTapped: panel.update("color", parent.modelData) }
                    }
                }
            }
            Text { text: "Горизонтальный интервал · " + (panel.options.horizontalSpacing || 0); color: theme.ink; font.pixelSize: 12 }
            FluentSlider {
                Layout.fillWidth: true; from: -4; to: 28; stepSize: 1; value: panel.options.horizontalSpacing || 0
                onPressedChanged: if (!pressed) panel.update("horizontalSpacing", Math.round(value))
            }
            Text { text: "Вертикальный интервал · " + (panel.options.verticalSpacing || 0); color: theme.ink; font.pixelSize: 12 }
            FluentSlider {
                Layout.fillWidth: true; from: -24; to: 80; stepSize: 1; value: panel.options.verticalSpacing || 0
                enabled: panel.options.mode === "stacked"
                onPressedChanged: if (!pressed) panel.update("verticalSpacing", Math.round(value))
            }
        }
    }
}
