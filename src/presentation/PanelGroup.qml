import QtQuick
import QtQuick.Controls
Grid {
    id: group
    required property int zone
    property bool vertical: false
    property bool logo: false
    columns: vertical ? 1 : 4
    spacing: 4
    component PanelButton: Button {
        width: 40; height: 38
        font.pixelSize: 14; font.bold: true
        icon.width: 18; icon.height: 18; icon.color: "#f7edf0"
        palette.buttonText: "#ececf5"
        background: Rectangle { radius: 11; color: parent.hovered ? "#67313b" : "#30313e" }
        ToolTip.visible: hovered
    }
    PanelButton {
        visible: group.logo
        Accessible.name: qsTr("Показать или скрыть настройки Umbrella")
        ToolTip.text: Accessible.name
        contentItem: Image { source: "qrc:/assets/umbrella.png"; sourceSize: Qt.size(40, 40); fillMode: Image.PreserveAspectFit; mipmap: true }
        onClicked: shell.toggleSettings()
    }
    Grid {
        visible: shell.moduleZones.workspaces === group.zone
        columns: group.vertical ? 1 : 3; spacing: 3
        PanelButton { icon.source: "qrc:/assets/icons/previous-light.svg"; Accessible.name: qsTr("Предыдущий рабочий стол"); ToolTip.text: Accessible.name; onClicked: shell.moveDesktop(-1) }
        PanelButton { icon.source: "qrc:/assets/icons/desktop-light.svg"; Accessible.name: qsTr("Все рабочие столы"); ToolTip.text: Accessible.name; onClicked: shell.showDesktops() }
        PanelButton { icon.source: "qrc:/assets/icons/next-light.svg"; Accessible.name: qsTr("Следующий рабочий стол"); ToolTip.text: Accessible.name; onClicked: shell.moveDesktop(1) }
    }
    PanelButton {
        visible: shell.moduleZones.keyboard === group.zone
        text: shell.keyboardLayout
        Accessible.name: qsTr("Сменить раскладку клавиатуры: ") + text
        ToolTip.text: Accessible.name
        onClicked: shell.cycleKeyboard()
    }
    PanelButton {
        visible: shell.moduleZones.volume === group.zone
        icon.source: "qrc:/assets/icons/volume-light.svg"; Accessible.name: qsTr("Параметры громкости Windows"); ToolTip.text: Accessible.name
        onClicked: shell.openSound()
    }
}
