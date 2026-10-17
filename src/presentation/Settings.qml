import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Rectangle {
    color: "#f4f5f7"
    // Sections are page components: new sections can be added to this model.
    readonly property var sections: [qsTr("Общие")]
    RowLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 24
        ColumnLayout {
            Layout.preferredWidth: 140; Layout.fillHeight: true
            Image { source: "qrc:/assets/umbrella.png"; Layout.preferredWidth: 64; Layout.preferredHeight: 64; fillMode: Image.PreserveAspectFit; sourceSize: Qt.size(64,64) }
            Label { text: "Umbrella Shell"; font.bold: true }
            Repeater { model: sections; delegate: Button { required property string modelData; required property int index; text: modelData; onClicked: pages.currentIndex = index } }
            Item { Layout.fillHeight: true }
        }
        StackLayout {
            id: pages; Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                Label { text: qsTr("Общие настройки"); font.pixelSize: 24; font.bold: true }
                CheckBox { text: qsTr("Использовать панель Umbrella"); checked: shell.replacement; onClicked: shell.replacement = checked }
                CheckBox { text: qsTr("Запускать при входе в Windows"); checked: shell.startup; onClicked: shell.startup = checked }
                Label { text: shell.status; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Label { text: qsTr("Панели Windows скрываются на время работы Umbrella.\nCtrl+Alt+U — закрыть Umbrella и восстановить Windows."); Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Item { Layout.fillHeight: true }
                Button { text: qsTr("Восстановить Windows и выйти"); onClicked: shell.quit() }
            }
        }
    }
}
