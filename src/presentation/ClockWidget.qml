import QtQuick
import QtQuick.Layouts

Item {
    id: clock
    required property var placement
    property bool editable: false
    readonly property var options: placement.settings || ({})
    readonly property bool stacked: options.mode === "stacked"
    readonly property bool blurredCard: options.style === "blur-card"
    readonly property bool blurredDigits: options.style === "blur-digits"
    readonly property color digits: options.color || "#FFF7F2"
    readonly property real tracking: options.horizontalSpacing || 0
    readonly property real rowGap: options.verticalSpacing || 0
    width: placement.width
    height: placement.height
    SettingsTheme { id: theme }

    WidgetSurface {
        anchors.fill: parent
        radius: 26
        visible: clock.blurredCard
        materialStyle: "acrylic"
        materialOwner: clock
        materialPlacement: clock.placement
    }
    Rectangle {
        anchors.fill: parent
        radius: 26
        color: "transparent"
        border.width: clock.editable ? 2 : 0
        border.color: theme.accent
    }

    Text {
        anchors.centerIn: parent
        visible: !clock.stacked && !clock.blurredDigits
        text: clockSystem.time.inline || "00:00"
        color: clock.digits
        font.family: "Segoe UI Variable Display"
        font.pixelSize: 112
        font.weight: Font.Black
        font.letterSpacing: clock.tracking
        renderType: Text.NativeRendering
    }
    MaterialText {
        anchors.centerIn: parent
        visible: !clock.stacked && clock.blurredDigits
        text: clockSystem.time.inline || "00:00"
        color: "white"
        font.family: "Segoe UI Variable Display"
        font.pixelSize: 112
        font.weight: Font.Black
        font.letterSpacing: clock.tracking
        renderType: Text.NativeRendering
        materialStyle: "acrylic"
        materialOwner: clock
        materialPlacement: clock.placement
    }
    Column {
        anchors.centerIn: parent
        visible: clock.stacked && !clock.blurredDigits
        spacing: -21 + clock.rowGap
        Repeater {
            model: [clockSystem.time.hours || "00", clockSystem.time.minutes || "00"]
            Text {
                required property string modelData
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData
                color: clock.digits
                font.family: "Segoe UI Variable Display"
                font.pixelSize: 116
                font.weight: Font.Black
                font.letterSpacing: clock.tracking
                renderType: Text.NativeRendering
            }
        }
    }
    Column {
        anchors.centerIn: parent
        visible: clock.stacked && clock.blurredDigits
        spacing: -21 + clock.rowGap
        Repeater {
            model: [clockSystem.time.hours || "00", clockSystem.time.minutes || "00"]
            MaterialText {
                required property string modelData
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData
                color: "white"
                font.family: "Segoe UI Variable Display"
                font.pixelSize: 116
                font.weight: Font.Black
                font.letterSpacing: clock.tracking
                renderType: Text.NativeRendering
                materialStyle: "acrylic"
                materialOwner: clock
                materialPlacement: clock.placement
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: clock.editable
        cursorShape: Qt.SizeAllCursor
        property real pressX
        property real pressY
        onPressed: function(mouse) {
            widgetSystem.selectInstance(clock.placement.instanceId)
            pressX = mouse.x; pressY = mouse.y
        }
        onPositionChanged: function(mouse) {
            if (pressed) { clock.x += mouse.x - pressX; clock.y += mouse.y - pressY }
        }
        onReleased: {
            const point = clock.mapToGlobal(0, 0)
            widgetSystem.moveInstance(clock.placement.instanceId, point.x, point.y)
        }
    }
}
