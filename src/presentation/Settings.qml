import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
Rectangle {
    id: root
    radius: shell.preferences.cornerRadius; color: root.theme.background; clip: true
    property int page: 0
    property bool panelExpanded: true
    readonly property var theme: shell.preferences.theme
    onPageChanged: pageFade.restart()
    NumberAnimation { id: pageFade; target: content; property: "opacity"; from: 0.35; to: 1; duration: 220; easing.type: Easing.OutCubic }
    readonly property color ink: root.theme.ink
    readonly property var modules: [
        { key: "workspaces", title: qsTr("Рабочие пространства"), icon: "desktop" },
        { key: "keyboard", title: qsTr("Клавиатура"), icon: "keyboard" },
        { key: "volume", title: qsTr("Громкость"), icon: "volume" }
    ]
    component TextLabel: Label { color: root.ink; font.family: shell.preferences.fontFamily; font.pixelSize: 12 }
    component Action: Button {
        font.family: shell.preferences.fontFamily; font.pixelSize: 11; font.bold: true
        palette.buttonText: root.ink
        background: Rectangle { radius: Math.min(10, shell.preferences.cornerRadius); color: parent.hovered ? Qt.tint(root.theme.surface, Qt.rgba(root.theme.accent.r, root.theme.accent.g, root.theme.accent.b, 0.22)) : root.theme.raised; Behavior on color { ColorAnimation { duration: 160 } } }
    }
    component Card: Rectangle { radius: Math.min(12, shell.preferences.cornerRadius); color: root.theme.surface }
    component PillSwitch: Switch {
        id: control
        width: parent.width; height: 38
        leftPadding: 0; rightPadding: 0; spacing: 12
        font.family: shell.preferences.fontFamily; font.pixelSize: 12
        indicator: Rectangle {
            implicitWidth: 46; implicitHeight: 26
            x: control.width - width
            y: (control.height - height) / 2
            radius: height / 2
            color: !control.enabled ? root.theme.raised : control.checked ? (control.down ? Qt.darker(root.theme.accent, 1.15) : root.theme.accent) : (control.hovered ? root.theme.raised : root.theme.raised)
            border.width: control.activeFocus ? 2 : 0
            border.color: Qt.darker(root.theme.accent, 1.15)
            Behavior on color { ColorAnimation { duration: 160 } }
            Rectangle {
                width: control.down ? 23 : 20; height: 20; radius: 10
                x: control.checked ? parent.width - width - 3 : 3
                y: 3; color: "white"
                Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                Behavior on width { NumberAnimation { duration: 120 } }
            }
        }
        contentItem: TextLabel {
            text: control.text; verticalAlignment: Text.AlignVCenter
            rightPadding: 58; wrapMode: Text.WordWrap
            opacity: control.enabled ? 1 : 0.5
        }
    }
    Rectangle {
        id: sidebar; width: 176; anchors.top: parent.top; anchors.bottom: parent.bottom
        color: root.theme.surface; radius: 16
        Rectangle { anchors.right: parent.right; width: 16; height: parent.height; color: root.theme.surface }
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
                    { title: qsTr("Тема"), icon: "theme", page: 4 },
                    { title: qsTr("Активность"), icon: "activity", page: 5 },
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
                        background: Rectangle { radius: Math.min(11, shell.preferences.cornerRadius); color: root.page === modelData.page ? root.theme.accent : (nav.hovered ? Qt.tint(root.theme.surface, Qt.rgba(root.theme.accent.r, root.theme.accent.g, root.theme.accent.b, 0.22)) : "transparent"); Behavior on color { ColorAnimation { duration: 180 } } }
                        contentItem: Row {
                            spacing: 7
                            Rectangle {
                                width: 25; height: 25; radius: 13; anchors.verticalCenter: parent.verticalCenter
                                color: root.page === modelData.page ? Qt.tint(root.theme.surface, Qt.rgba(root.theme.accent.r, root.theme.accent.g, root.theme.accent.b, 0.22)) : root.theme.raised
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
                            width: parent.width - 16; height: 28; radius: Math.min(10, shell.preferences.cornerRadius); color: Qt.tint(root.theme.surface, Qt.rgba(root.theme.accent.r, root.theme.accent.g, root.theme.accent.b, 0.22))
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
                TextLabel { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Ваша панель. Ваше пространство."); color: root.theme.muted }
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
                        PillSwitch { text: qsTr("Использовать панель Umbrella"); checked: shell.replacement; onClicked: shell.replacement = checked }
                        PillSwitch { text: qsTr("Запускать при входе в Windows"); checked: shell.startup; onClicked: shell.startup = checked }
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
                        palette.highlight: root.theme.accent
                        model: [qsTr("Сверху"), qsTr("Снизу"), qsTr("Слева"), qsTr("Справа")]
                        currentIndex: ["top", "bottom", "left", "right"].indexOf(shell.panelEdge)
                        onActivated: shell.panelEdge = ["top", "bottom", "left", "right"][currentIndex]
                    }
                }
                Card {
                    Layout.fillWidth: true; implicitHeight: config.implicitHeight + 22
                    ModuleConfiguration {
                        id: config
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 11
                        modules: root.modules; dragSurface: dragOverlay
                    }
                }
                Card {
                    Layout.fillWidth: true; implicitHeight: 64
                    TextLabel { x: 12; y: 10; text: qsTr("Количество пространств"); font.bold: true }
                    TextLabel { x: 12; y: 34; text: qsTr("При изменении все окна вернутся в первую группу"); font.pixelSize: 10 }
                    SpinBox {
                        anchors.right: parent.right; anchors.rightMargin: 10
                        anchors.top: parent.top; anchors.topMargin: 7; width: 110
                        from: 1; to: 10; value: shell.workspaceCount; editable: false
                        palette.highlight: root.theme.accent
                        onValueModified: shell.workspaceCount = value
                        Accessible.name: qsTr("Количество рабочих пространств")
                    }
                }
                TextLabel { text: qsTr("Кружки — группы окон Umbrella каждого монитора. Заполненный — есть окна, красный — активен, пустой — свободен. Системные бинды Windows не меняются."); Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 10 }
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
        ThemePage { visible: root.page === 4; anchors.fill: parent; anchors.margins: 12 }
        ActivityPage { visible: root.page === 5; anchors.fill: parent; anchors.margins: 12 }
    }
    Item { id: dragOverlay; anchors.fill: parent; z: 1000 }
}
