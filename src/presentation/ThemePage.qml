import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
ScrollView {
    id: page
    contentWidth: availableWidth
    readonly property var prefs: shell.preferences
    readonly property var theme: prefs.theme
    component Caption: Label { color: page.theme.ink; font.family: page.prefs.fontFamily; font.pixelSize: 11 }
    component Tile: Rectangle { radius: page.prefs.cornerRadius; color: page.theme.surface; Layout.fillWidth: true }
    FileDialog { id: wallpaperPicker; title: qsTr("Выбрать обои Windows"); nameFilters: [qsTr("Изображения (*.png *.jpg *.jpeg *.bmp)")]; onAccepted: page.prefs.setWallpaper(selectedFile) }
    FolderDialog { id: folderPicker; title: qsTr("Папка с обоями"); onAccepted: page.prefs.setWallpaperFolder(selectedFolder) }
    FileDialog { id: fontPicker; title: qsTr("Добавить шрифт для Umbrella"); nameFilters: [qsTr("Шрифты (*.ttf *.otf)")]; onAccepted: page.prefs.loadFont(selectedFile) }
    ColorDialog { id: colorPicker; title: qsTr("Цвет акцента"); selectedColor: "#d9263b" }
    Dialog {
        id: customDialog; title: qsTr("Собственная тема"); modal: true
        anchors.centerIn: parent; width: 300
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: page.prefs.addTheme(customName.text, colorPicker.selectedColor, darkTheme.checked)
        ColumnLayout {
            width: parent.width
            UiField { id: customName; placeholderText: qsTr("Название"); Layout.fillWidth: true; maximumLength: 40 }
            PillSwitch { id: darkTheme; text: qsTr("Тёмная основа"); Layout.fillWidth: true }
            UiButton { text: qsTr("Цвет акцента"); onClicked: colorPicker.open() }
        }
    }
    ColumnLayout {
        width: page.availableWidth; spacing: 8
        Tile {
            implicitHeight: 124; clip: true
            Image { anchors.fill: parent; source: page.prefs.wallpaper; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 900 }
            Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0.4; color: "transparent" } GradientStop { position: 1; color: "#bb15151a" } } }
            UiButton { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 9; text: qsTr("Выбрать обои"); accent: true; onClicked: wallpaperPicker.open() }
            Caption { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; width: parent.width / 2; horizontalAlignment: Text.AlignRight; elide: Text.ElideMiddle; text: decodeURIComponent(page.prefs.wallpaper.toString().split('/').pop()); color: "white" }
        }
        Tile {
            implicitHeight: 48
            Caption { x: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Папка с обоями") }
            UiButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.rightMargin: 8; width: 230; text: page.prefs.wallpaperFolder.toString() || qsTr("Выбрать папку…"); onClicked: folderPicker.open() }
        }
        Flow {
            Layout.fillWidth: true; spacing: 6
            Repeater {
                model: page.prefs.wallpapers
                delegate: Item {
                    required property var modelData
                    width: 96; height: 64
                    Image { anchors.fill: parent; source: modelData.url; sourceSize: Qt.size(160, 100); fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.prefs.setWallpaper(parent.modelData.url) }
                    Accessible.name: modelData.name
                }
            }
        }
        Tile {
            implicitHeight: 48
            Caption { x: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Шрифт") }
            RowLayout {
                anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                UiButton { text: "+"; implicitWidth: 32; onClicked: fontPicker.open(); Accessible.name: qsTr("Загрузить шрифт") }
                UiCombo { implicitWidth: 215; model: page.prefs.fonts; currentIndex: model.indexOf(page.prefs.fontFamily); onActivated: page.prefs.fontFamily = currentText }
            }
        }
        Tile {
            implicitHeight: 48
            Caption { x: 10; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Радиус скругления") }
            UiSpin { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; from: 0; to: 28; value: page.prefs.cornerRadius; onValueModified: page.prefs.cornerRadius = value }
        }
        Tile {
            implicitHeight: paletteContent.implicitHeight + 20
            ColumnLayout {
                id: paletteContent; anchors.fill: parent; anchors.margins: 10; spacing: 8
                RowLayout {
                    Caption { text: qsTr("Цветовые темы"); font.bold: true; Layout.fillWidth: true }
                    UiButton { text: "+"; implicitWidth: 30; onClicked: customDialog.open(); Accessible.name: qsTr("Создать тему") }
                    UiField { id: search; placeholderText: qsTr("Поиск тем…"); Layout.preferredWidth: 130 }
                }
                GridLayout {
                    columns: 2; Layout.fillWidth: true; columnSpacing: 8; rowSpacing: 8
                    Repeater {
                        model: page.prefs.themes
                        delegate: Rectangle {
                            required property var modelData
                            visible: modelData.name.toLowerCase().includes(search.text.toLowerCase())
                            Layout.fillWidth: true; implicitHeight: 42; radius: page.prefs.cornerRadius
                            color: modelData.background; border.width: page.theme.name === modelData.name ? 2 : 0; border.color: modelData.accent
                            Caption { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: modelData.ink }
                            Row { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; spacing: 3
                                Repeater { model: [modelData.ink, modelData.muted, modelData.accent]; delegate: Rectangle { required property string modelData; width: 7; height: 7; radius: 4; color: modelData } }
                            }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.prefs.selectTheme(parent.modelData.name) }
                            Behavior on color { ColorAnimation { duration: 180 } }
                        }
                    }
                }
            }
        }
        Caption { text: page.prefs.error; visible: text.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: page.theme.accent }
    }
}
