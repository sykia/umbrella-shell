import QtQuick
import QtQuick.Window
import QtQml

Item {
    WidgetEditor { id: editor }
    Instantiator {
        model: widgetSystem.instances
        delegate: Window {
            id: surface
            required property var modelData
            readonly property bool editingHere: widgetSystem.editing && widgetSystem.selectedMonitorId === modelData.screenId
            property bool attached: false
            objectName: "clock_" + modelData.instanceId
            title: "Umbrella · Clock"
            flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
            transientParent: null
            color: "transparent"
            width: modelData.width; height: modelData.height
            visible: attached && !editingHere

            function attach() {
                if (editingHere) return
                attached = false
                attached = widgetSystem.attachDesktop(surface, modelData.instanceId)
            }
            Component.onCompleted: Qt.callLater(attach)
            onModelDataChanged: Qt.callLater(attach)
            onEditingHereChanged: if (!editingHere) Qt.callLater(attach)
            Connections { target: widgetSystem; function onDesktopChanged() { surface.attach() } }

            Loader {
                id: visual
                parent: surface.editingHere ? editor.contentItem : surface.contentItem
                x: surface.editingHere ? surface.modelData.globalX - editor.x : 0
                y: surface.editingHere ? surface.modelData.globalY - editor.y : 0
                width: surface.modelData.width; height: surface.modelData.height
                sourceComponent: clockVisual
                Component { id: clockVisual; ClockWidget { placement: surface.modelData; editable: surface.editingHere } }
            }
        }
    }
}
