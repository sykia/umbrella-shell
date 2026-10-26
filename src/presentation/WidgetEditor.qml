import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: editor
    objectName: "widgetEditor"
    title: "Umbrella — Widgets"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    transientParent: null
    color: "transparent"
    minimumWidth: 720; minimumHeight: 520
    property bool backdropAvailable: false
    property bool entered: false
    SettingsTheme { id: theme }
    function updateMaterial() { backdropAvailable = widgetSystem.prepareWindow(editor, theme.transparency, theme.dark) }
    function enter() { entered = false; updateMaterial(); requestActivate(); Qt.callLater(function() { editor.entered = true }) }
    Connections {
        target: widgetSystem
        function onEditChanged() { if (widgetSystem.editing) editor.enter(); else { editor.entered = false; editor.hide() } }
    }
    Connections { target: theme; function onDarkChanged() { editor.updateMaterial() } function onTransparencyChanged() { editor.updateMaterial() } }
    Component.onCompleted: if (widgetSystem.editing) enter()
    onClosing: function(close) { close.accepted = false; widgetSystem.closeEditor() }

    Rectangle {
        anchors.fill: parent
        color: editor.backdropAvailable ? Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.08) : theme.background
        focus: true
        Keys.onEscapePressed: widgetSystem.closeEditor()
        Rectangle {
            id: topBar
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 82
            color: editor.backdropAvailable ? Qt.rgba(theme.card.r, theme.card.g, theme.card.b, 0.78) : theme.card
            border.color: theme.line
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 30; anchors.rightMargin: 26; spacing: 14
                Rectangle {
                    Layout.preferredWidth: 42; Layout.preferredHeight: 42; radius: 12; color: theme.tint
                    Text { anchors.centerIn: parent; text: "\uE787"; font.family: "Segoe Fluent Icons"; font.pixelSize: 20; color: theme.accent }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text { text: "Часы"; color: theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold }
                    Text { text: widgetSystem.selectedMonitorLabel + " · перетаскивайте часы мышью"; color: theme.muted; font.pixelSize: 12 }
                }
                FluentButton { text: "Готово"; implicitWidth: 110; onClicked: widgetSystem.closeEditor() }
            }
        }

        Item {
            id: workspace
            anchors.top: topBar.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            opacity: editor.entered ? 1 : 0; scale: editor.entered ? 1 : 0.99
            Behavior on opacity { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }

            Rectangle {
                id: emptyCard
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: widgetSystem.selectedClock.instanceId ? -170 : 0
                width: 410; height: 170; radius: 22
                color: editor.backdropAvailable ? Qt.rgba(theme.card.r, theme.card.g, theme.card.b, 0.78) : theme.card
                border.color: theme.line
                visible: widgetSystem.instances.filter(item => item.screenId === widgetSystem.selectedMonitorId).length === 0
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 24; spacing: 10
                    Text { Layout.fillWidth: true; text: "Добавьте часы"; color: theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
                    Text { Layout.fillWidth: true; text: "Два формата, два стиля и настраиваемые интервалы."; color: theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        FluentButton { text: "+  00:00"; onClicked: widgetSystem.addClock("inline") }
                        FluentButton { text: "+  00 / 00"; onClicked: widgetSystem.addClock("stacked") }
                    }
                }
            }

            ClockSettings {
                id: settingsPanel
                anchors.top: parent.top; anchors.bottom: bottomDock.top
                anchors.topMargin: 22; anchors.bottomMargin: 16
                width: 330
                record: widgetSystem.selectedClock
                visible: !!record.instanceId
                opacity: visible ? 1 : 0
                x: visible ? parent.width - width - 22 : parent.width + 12
                Behavior on x { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: theme.fast } }
            }

            Rectangle {
                id: bottomDock
                anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 24
                width: 390; height: 72; radius: 20
                color: editor.backdropAvailable ? Qt.rgba(theme.card.r, theme.card.g, theme.card.b, 0.84) : theme.card
                border.color: theme.line
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8
                    FluentButton { Layout.fillWidth: true; text: "+ Обычные"; onClicked: widgetSystem.addClock("inline") }
                    FluentButton { Layout.fillWidth: true; text: "+ Вертикальные"; onClicked: widgetSystem.addClock("stacked") }
                }
            }
        }
    }
}
