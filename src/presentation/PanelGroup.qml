import QtQuick
import QtQuick.Controls
Grid {
    id: group
    required property int zone
    property bool vertical: false
    property bool logo: false
    columns: vertical ? 1 : 4
    spacing: 6
    component PanelButton: Button {
        width: 26; height: 26
        font.family: "Segoe UI"; font.pixelSize: 11; font.bold: true
        icon.width: 15; icon.height: 15; icon.color: "#585365"
        palette.buttonText: "#585365"
        background: Rectangle {
            radius: 13; color: parent.down ? "#d9263b" : (parent.hovered ? "#efcbd2" : "#dfe1e9")
            Behavior on color { ColorAnimation { duration: 140 } }
        }
        ToolTip.visible: hovered
    }
    PanelButton {
        visible: group.logo
        Accessible.name: qsTr("Показать или скрыть настройки Umbrella")
        ToolTip.text: Accessible.name
        contentItem: Image { source: "qrc:/assets/umbrella.png"; sourceSize: Qt.size(32, 32); fillMode: Image.PreserveAspectFit; mipmap: true }
        onClicked: shell.toggleSettings()
    }
    Grid {
        visible: shell.moduleZones.workspaces === group.zone
        columns: group.vertical ? 1 : 3; spacing: 3
        PanelButton { icon.source: "qrc:/assets/icons/previous.svg"; Accessible.name: qsTr("Предыдущий рабочий стол"); ToolTip.text: Accessible.name; onClicked: shell.moveDesktop(-1) }
        PanelButton { icon.source: "qrc:/assets/icons/desktop.svg"; Accessible.name: qsTr("Все рабочие столы"); ToolTip.text: Accessible.name; onClicked: shell.showDesktops() }
        PanelButton { icon.source: "qrc:/assets/icons/next.svg"; Accessible.name: qsTr("Следующий рабочий стол"); ToolTip.text: Accessible.name; onClicked: shell.moveDesktop(1) }
    }
    PanelButton {
        visible: shell.moduleZones.keyboard === group.zone
        width: group.vertical ? 26 : 58
        text: shell.keyboardLayout
        icon.source: group.vertical ? "" : "qrc:/assets/icons/keyboard.svg"
        leftPadding: 6; rightPadding: 6; spacing: 4
        Accessible.name: qsTr("Сменить раскладку клавиатуры: ") + text
        ToolTip.text: Accessible.name
        onClicked: shell.cycleKeyboard()
    }
    PanelButton {
        visible: shell.moduleZones.volume === group.zone
        width: group.vertical ? 26 : 68
        text: group.vertical ? "" : qsTr("Звук")
        leftPadding: 6; rightPadding: 6; spacing: 4
        icon.source: "qrc:/assets/icons/volume.svg"
        Accessible.name: qsTr("Параметры громкости Windows"); ToolTip.text: Accessible.name
        onClicked: shell.openSound()
    }
}
