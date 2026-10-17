import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 32; leftPadding: 12; rightPadding: 26
    font.family: shell.preferences.fontFamily; font.pixelSize: 11
    contentItem: Label { text: control.displayText; color: shell.preferences.theme.ink; font: control.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    indicator: Image { x: control.width - 21; y: (control.height - height) / 2; width: 12; height: 12; rotation: 90; source: "qrc:/assets/icons/next.svg"; sourceSize: Qt.size(24, 24) }
    background: Rectangle {
        radius: Math.max(8, Math.min(16, shell.preferences.cornerRadius))
        color: shell.preferences.theme.raised
        border.width: control.activeFocus || control.hovered ? 1 : 0; border.color: shell.preferences.theme.accent
    }
    popup: Popup {
        y: control.height + 4; width: control.width; padding: 6
        implicitHeight: Math.min(240, contentItem.implicitHeight + 12)
        background: Rectangle { radius: 12; color: shell.preferences.theme.surface; border.width: 1; border.color: shell.preferences.theme.raised }
        contentItem: ListView {
            clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
