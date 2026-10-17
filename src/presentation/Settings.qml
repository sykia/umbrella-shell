import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
Rectangle {
    id: root
    radius: 16; color: "#eef0f6"; clip: true
    property int page: 0
    property bool panelExpanded: true
    readonly property color ink: "#50516b"
    readonly property var modules: [
        { key: "workspaces", title: qsTr("Рабочие пространства"), icon: "◐" },
        { key: "keyboard", title: qsTr("Клавиатура"), icon: "⌨" },
        { key: "volume", title: qsTr("Громкость"), icon: "♪" }
    ]
    component TextLabel: Label { color: root.ink; font.family: "Segoe UI"; font.pixelSize: 12 }
    component Action: Button {
        font.family: "Segoe UI"; font.pixelSize: 11; font.bold: true
        palette.buttonText: root.ink
        background: Rectangle { radius: 10; color: parent.hovered ? "#c5b4e4" : "#d0d3df" }
    }
    component Card: Rectangle { radius: 12; color: "#e1e4ec" }
    Rectangle {
        id: sidebar; width: 176; anchors.top: parent.top; anchors.bottom: parent.bottom
        color: "#e1e4ec"; radius: 16
        Rectangle { anchors.right: parent.right; width: 16; height: parent.height; color: "#e1e4ec" }
        Column {
            anchors.fill: parent; anchors.margins: 10; spacing: 5
            Item {
                width: parent.width; height: 34
                TextLabel { text: qsTr("Настройки"); font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                DragHandler { target: null; onActiveChanged: if (active) root.Window.window.startSystemMove() }
            }
            Repeater {
                model: [
                    { title: qsTr("Добро пожаловать"), icon: "⌂", page: 0 },
                    { title: qsTr("Общие"), icon: "⚙", page: 1 },
                    { title: qsTr("Панель"), icon: "▣", page: 2 },
                    { title: qsTr("О программе"), icon: "ⓘ", page: 3 }
                ]
                delegate: Column {
                    required property var modelData
                    width: parent.width; spacing: 3
                    Button {
                        id: nav
                        width: parent.width; height: 36
                        onClicked: { root.page = modelData.page; if (modelData.page === 2) root.panelExpanded = true }
                        background: Rectangle { radius: 11; color: root.page === modelData.page ? "#8b35ed" : (nav.hovered ? "#d4d7e2" : "transparent") }
                        contentItem: Row {
                            spacing: 7
                            Rectangle {
                                width: 25; height: 25; radius: 13; anchors.verticalCenter: parent.verticalCenter
                                color: root.page === modelData.page ? "#aa6cf1" : "#d5d9e3"
                                TextLabel { anchors.centerIn: parent; text: modelData.icon; color: root.page === modelData.page ? "white" : "#87899f"; font.pixelSize: 16 }
                            }
                            TextLabel { text: modelData.title; anchors.verticalCenter: parent.verticalCenter; color: root.page === modelData.page ? "white" : root.ink; font.pixelSize: 11; font.bold: root.page === modelData.page }
                        }
                    }
                    Row {
                        visible: modelData.page === 2 && root.page === 2 && root.panelExpanded
                        width: parent.width; height: 30; spacing: 7
                        Rectangle { width: 1; height: 27; color: "#b7b9c8"; x: 12 }
                        Rectangle {
                            width: parent.width - 16; height: 28; radius: 10; color: "#d4b7f5"
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
                TextLabel { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Ваша панель. Ваше пространство."); color: "#77798f" }
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
                        CheckBox { text: qsTr("Использовать панель Umbrella"); checked: shell.replacement; onClicked: shell.replacement = checked; palette.windowText: root.ink; font.pixelSize: 12 }
                        CheckBox { text: qsTr("Запускать при входе в Windows"); checked: shell.startup; onClicked: shell.startup = checked; palette.windowText: root.ink; font.pixelSize: 12 }
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
                                Layout.fillWidth: true; implicitHeight: chips.implicitHeight + 28; radius: 10; color: "#eef0f6"; border.color: "#d4d7e1"
                                TextLabel { x: 9; y: 5; text: modelData; font.pixelSize: 10; font.bold: true }
                                Flow {
                                    id: chips; x: 8; y: 23; width: parent.width - 16; spacing: 5
                                    Repeater {
                                        model: root.modules
                                        delegate: Action {
                                            required property var modelData
                                            visible: shell.moduleZones[modelData.key] === zone
                                            text: modelData.icon + "  " + modelData.title
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
