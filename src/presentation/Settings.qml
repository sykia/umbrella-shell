import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
Rectangle {
    id: root
    radius: 16; color: "#f7f2f3"; clip: true
    property int page: 0
    property bool panelExpanded: true
    onPageChanged: pageFade.restart()
    NumberAnimation { id: pageFade; target: content; property: "opacity"; from: 0.35; to: 1; duration: 220; easing.type: Easing.OutCubic }
    readonly property color ink: "#4c4145"
    readonly property var modules: [
        { key: "workspaces", title: qsTr("Рабочие пространства"), icon: "desktop" },
        { key: "keyboard", title: qsTr("Клавиатура"), icon: "keyboard" },
        { key: "volume", title: qsTr("Громкость"), icon: "volume" }
    ]
    component TextLabel: Label { color: root.ink; font.family: "Segoe UI"; font.pixelSize: 12 }
    component Action: Button {
        font.family: "Segoe UI"; font.pixelSize: 11; font.bold: true
        palette.buttonText: root.ink
        background: Rectangle { radius: 10; color: parent.hovered ? "#edb7bf" : "#e4d5d8"; Behavior on color { ColorAnimation { duration: 160 } } }
    }
    component Card: Rectangle { radius: 12; color: "#ece4e6" }
    Rectangle {
        id: sidebar; width: 176; anchors.top: parent.top; anchors.bottom: parent.bottom
        color: "#ece4e6"; radius: 16
        Rectangle { anchors.right: parent.right; width: 16; height: parent.height; color: "#ece4e6" }
        Column {
            anchors.fill: parent; anchors.margins: 10; spacing: 5
            Item {
                width: parent.width; height: 34
                TextLabel { text: qsTr("Настройки"); font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                DragHandler { target: null; onActiveChanged: if (active) root.Window.window.startSystemMove() }
            }
            Repeater {
                model: [
                    { title: qsTr("Добро пожаловать"), icon: "home", page: 0 },
                    { title: qsTr("Общие"), icon: "general", page: 1 },
                    { title: qsTr("Панель"), icon: "panel", page: 2 },
                    { title: qsTr("О программе"), icon: "info", page: 3 }
                ]
                delegate: Column {
                    required property var modelData
                    width: parent.width; spacing: 3
                    Button {
                        id: nav
                        width: parent.width; height: 36
                        Accessible.name: modelData.title
                        scale: hovered ? 1.015 : 1
                        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        onClicked: { root.page = modelData.page; if (modelData.page === 2) root.panelExpanded = true }
                        background: Rectangle { radius: 11; color: root.page === modelData.page ? "#d9263b" : (nav.hovered ? "#e5d7db" : "transparent"); Behavior on color { ColorAnimation { duration: 180 } } }
                        contentItem: Row {
                            spacing: 7
                            Rectangle {
                                width: 25; height: 25; radius: 13; anchors.verticalCenter: parent.verticalCenter
                                color: root.page === modelData.page ? "#ec6978" : "#dfd3d7"
                                Behavior on color { ColorAnimation { duration: 180 } }
                                Image { anchors.centerIn: parent; width: 16; height: 16; source: "qrc:/assets/icons/" + modelData.icon + (root.page === modelData.page ? "-light" : "") + ".svg"; sourceSize: Qt.size(24, 24) }
                            }
                            TextLabel { text: modelData.title; anchors.verticalCenter: parent.verticalCenter; color: root.page === modelData.page ? "white" : root.ink; font.pixelSize: 11; font.bold: root.page === modelData.page }
                        }
                    }
                    Row {
                        visible: modelData.page === 2 && root.page === 2 && root.panelExpanded
                        width: parent.width; height: 30; spacing: 7
                        Rectangle { width: 1; height: 27; color: "#c5b3ba"; x: 12 }
                        Rectangle {
                            width: parent.width - 16; height: 28; radius: 10; color: "#f1b9c2"
                            TextLabel { anchors.centerIn: parent; text: qsTr("Основные и модули"); font.pixelSize: 10; font.bold: true }
                        }
                    }
                }
            }
        }
    }
    Item {
        id: content
        anchors.left: sidebar.right; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        Item {
            anchors.fill: parent; visible: root.page === 0
            Column {
                anchors.centerIn: parent; spacing: 14
                UmbrellaLogo { anchors.horizontalCenter: parent.horizontalCenter }
                TextLabel { anchors.horizontalCenter: parent.horizontalCenter; text: "Umbrella Shell"; font.pixelSize: 27; font.bold: true }
                TextLabel { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Ваша панель. Ваше пространство."); color: "#93828a" }
                Action { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("О программе"); height: 32; width: 126; onClicked: root.page = 3 }
            }
        }
        ScrollView {
            visible: root.page === 1; anchors.fill: parent; anchors.margins: 14; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 10
                TextLabel { text: qsTr("Общие"); font.bold: true; font.pixelSize: 21 }
                Card {
                    Layout.fillWidth: true; implicitHeight: 96
                    Column {
                        anchors.fill: parent; anchors.margins: 10
                        CheckBox { text: qsTr("Использовать панель Umbrella"); checked: shell.replacement; onClicked: shell.replacement = checked; palette.windowText: root.ink; palette.highlight: "#d9263b"; font.pixelSize: 12 }
                        CheckBox { text: qsTr("Запускать при входе в Windows"); checked: shell.startup; onClicked: shell.startup = checked; palette.windowText: root.ink; palette.highlight: "#d9263b"; font.pixelSize: 12 }
                    }
                }
                TextLabel { text: shell.status; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                TextLabel { text: qsTr("Ctrl+Alt+U — аварийное восстановление Windows. Кнопка Umbrella на панели показывает и скрывает настройки."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Action { text: qsTr("Восстановить Windows и выйти"); height: 34; Layout.preferredHeight: 34; onClicked: shell.quit() }
            }
        }
        ScrollView {
            visible: root.page === 2; anchors.fill: parent; anchors.margins: 12; contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: 9
                Card {
                    Layout.fillWidth: true; implicitHeight: 58
                    TextLabel { x: 12; y: 10; text: qsTr("Расположение панели"); font.bold: true }
                    TextLabel { x: 12; y: 30; text: qsTr("Одинаково на всех мониторах"); font.pixelSize: 10 }
                    ComboBox {
                        anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: 130
                        palette.highlight: "#d9263b"
                        model: [qsTr("Сверху"), qsTr("Снизу"), qsTr("Слева"), qsTr("Справа")]
                        currentIndex: ["top", "bottom", "left", "right"].indexOf(shell.panelEdge)
                        onActivated: shell.panelEdge = ["top", "bottom", "left", "right"][currentIndex]
                    }
                }
                Card {
                    Layout.fillWidth: true; implicitHeight: config.implicitHeight + 22
                    ColumnLayout {
                        id: config; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 11; spacing: 7
                        TextLabel { text: qsTr("Конфигурация панели"); font.bold: true }
                        TextLabel { text: qsTr("Нажмите модуль для переноса в следующую зону. ПКМ — убрать в доступные."); Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 10 }
                        Repeater {
                            model: [qsTr("Доступные"), qsTr("Слева"), qsTr("По центру"), qsTr("Справа")]
                            delegate: Rectangle {
                                required property string modelData
                                required property int index
                                readonly property int zone: index - 1
                                Layout.fillWidth: true; implicitHeight: chips.implicitHeight + 28; radius: 10; color: "#f7f2f3"; border.color: "#dfd1d6"
                                TextLabel { x: 9; y: 5; text: modelData; font.pixelSize: 10; font.bold: true }
                                Flow {
                                    id: chips; x: 8; y: 23; width: parent.width - 16; spacing: 5
                                    Repeater {
                                        model: root.modules
                                        delegate: Action {
                                            required property var modelData
                                            visible: shell.moduleZones[modelData.key] === zone
                                            text: modelData.title
                                            icon.source: "qrc:/assets/icons/" + modelData.icon + ".svg"
                                            icon.color: root.ink; icon.width: 14; icon.height: 14
                                            height: 27; width: implicitWidth + 12
                                            onClicked: shell.setModuleZone(modelData.key, zone === 2 ? -1 : zone + 1)
                                            MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: shell.setModuleZone(modelData.key, -1) }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                TextLabel { text: qsTr("Рабочие столы: кнопки ‹ / › и обзор. Штатные Win+Ctrl+←/→ и Win+цифры сохраняются."); Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 10 }
            }
        }
        ColumnLayout {
            visible: root.page === 3; anchors.fill: parent; anchors.margins: 26; spacing: 12
            UmbrellaLogo { animated: false; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 90; Layout.preferredHeight: 90 }
            TextLabel { text: "Umbrella Shell"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
            TextLabel { text: qsTr("Панель Windows на Qt Quick / Direct3D 11.\nВизуальная компоновка вдохновлена Serpantinum; код и логотип — Umbrella."); Layout.fillWidth: true; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            Item { Layout.fillHeight: true }
            Action { text: qsTr("Общие настройки"); Layout.alignment: Qt.AlignHCenter; height: 32; onClicked: root.page = 1 }
        }
    }
}
