import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    objectName: "taskbarPage"
    property string edge: system ? system.edge : "bottom"
    property bool detached: system ? (system.detached || system.islands) : false
    property real transparency: system ? system.transparency : 35
    property int thickness: system ? system.thickness : 48
    property bool islands: system ? system.islands : false
    property real widthPercent: system ? system.widthPercent : 100
    readonly property string panelStyle: islands ? "islands" : detached ? "detached" : "attached"
    property int separateThickness: system && system.detached && !system.islands ? system.thickness : 48
    property int islandThickness: system && system.islands ? system.thickness : 72
    function selectStyle(style) {
        if (style === panelStyle || (style === "islands" && !widthAvailable)) return
        if (panelStyle === "detached") separateThickness = thickness
        if (panelStyle === "islands") islandThickness = thickness
        islands = style === "islands"
        detached = style !== "attached"
        thickness = islands ? Math.max(64, islandThickness) : detached ? separateThickness : 48
        if (islands) autoHide = false
        else if (autoHide) thickness = 48
        scheduleApply()
    }
    readonly property bool vertical: edge === "left" || edge === "right"
    readonly property bool widthAvailable: system ? system.widthAvailable : false
    readonly property bool sidesAvailable: system ? system.sidesAvailable : false
    readonly property bool canApply: (!vertical || sidesAvailable) && (!detached || Math.round(widthPercent) === 100 || widthAvailable)
    property var system
    property bool autoHide: system ? system.autoHide : false
    property bool readyToApply: false
    property bool applyQueued: false
    readonly property color accent: "#e7131b"

    Component.onCompleted: readyToApply = true
    function scheduleApply() {
        if (!readyToApply || !canApply || !system || !system.available) return
        applyQueued = true
        liveApply.restart()
    }
    Timer {
        id: liveApply
        interval: 180
        repeat: false
        onTriggered: {
            if (!page.applyQueued || !page.canApply || !page.system || !page.system.available) return
            if (page.system.pending) return
            page.applyQueued = false
            page.system.apply(page.edge, page.detached, Math.round(page.transparency),
                              page.autoHide, Math.round(page.widthPercent), page.thickness, page.islands)
        }
    }
    Connections {
        target: page.system
        function onChanged() {
            if (page.applyQueued && !page.system.pending) liveApply.restart()
        }
    }

    component Caption: Text {
        color: "white"
        font.family: "Segoe UI"
        font.pixelSize: 13
        renderType: Text.NativeRendering
    }
    component Choice: Button {
        id: choice
        property bool selected: false
        hoverEnabled: true
        font.family: "Segoe UI"
        font.pixelSize: 12
        implicitHeight: 34
        scale: pressed ? 0.97 : hovered ? 1.025 : 1
        Behavior on scale { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
        background: Rectangle {
            radius: 11
            color: choice.selected ? page.accent : choice.hovered ? "#30ffffff" : "#15ffffff"
            Behavior on color { ColorAnimation { duration: 180 } }
        }
        contentItem: Text {
            text: choice.text; font: choice.font; color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    component Toggle: Switch {
        id: toggle
        text: ""
        implicitWidth: 48; implicitHeight: 30
        padding: 0
        indicator: Rectangle {
            width: 48; height: 28; radius: 14
            anchors.centerIn: parent
            color: toggle.checked ? page.accent : "#35ffffff"
            Behavior on color { ColorAnimation { duration: 180 } }
            Rectangle {
                width: 20; height: 20; radius: 10; y: 4
                x: toggle.checked ? 24 : 4
                color: "white"
                Behavior on x { NumberAnimation { duration: 190; easing.type: Easing.OutCubic } }
                scale: toggle.down ? 0.9 : 1
                Behavior on scale { NumberAnimation { duration: 120 } }
            }
        }
        contentItem: Item {}
    }

    ScrollView {
        id: pageScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ColumnLayout {
            width: parent.width
            spacing: 12
            Caption { text: "Taskbar"; font.pixelSize: 24; font.weight: Font.DemiBold }
            Caption {
                text: "Панель задач"
                opacity: 0.65; font.pixelSize: 11
                Layout.topMargin: -8
            }

            TaskbarConfiguration { Layout.fillWidth: true; system: page.system; scrollTarget: pageScroll.contentItem }

            Rectangle {
                id: desktop
                Layout.fillWidth: true
                implicitHeight: 114
                radius: 14; clip: true
                gradient: Gradient {
                    GradientStop { position: 0; color: "#485e62" }
                    GradientStop { position: 1; color: "#252b43" }
                }
                readonly property bool vertical: page.edge === "left" || page.edge === "right"
                readonly property real gap: page.detached ? 9 : 0
                readonly property real thickness: 22 * page.thickness / 48
                Rectangle {
                    x: desktop.width * 0.18; y: 22
                    width: desktop.width * 0.32; height: 47; radius: 7
                    color: "#12ffffff"
                }
                Rectangle {
                    x: desktop.width * 0.42; y: 36
                    width: desktop.width * 0.35; height: 52; radius: 7
                    color: "#12ffffff"
                }
                Rectangle {
                    id: previewBar
                    objectName: "taskbarPreview"
                    readonly property bool hidden: page.autoHide && !previewHover.hovered
                    x: desktop.vertical ? (page.edge === "right" ? desktop.width - width - desktop.gap : desktop.gap)
                       : (desktop.width - width) / 2
                    y: page.edge === "bottom" ? desktop.height - height - desktop.gap : desktop.gap
                    width: desktop.vertical ? desktop.thickness
                           : (desktop.width - 2 * desktop.gap) * (page.detached ? page.widthPercent / 100 : 1)
                    height: desktop.vertical ? desktop.height - 2 * desktop.gap : desktop.thickness
                    radius: page.detached ? 9 : 0
                    color: page.islands ? "transparent" : Qt.rgba(0.9, 0.92, 1, 1 - page.transparency / 100)
                    opacity: hidden ? 0.15 : 1
                    Behavior on x { NumberAnimation { duration: 260; easing.type: Easing.InOutCubic } }
                    Behavior on y { NumberAnimation { duration: 260; easing.type: Easing.InOutCubic } }
                    Behavior on width { NumberAnimation { duration: 260; easing.type: Easing.InOutCubic } }
                    Behavior on height { NumberAnimation { duration: 260; easing.type: Easing.InOutCubic } }
                    Behavior on radius { NumberAnimation { duration: 220 } }
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Behavior on opacity { NumberAnimation { duration: 240 } }
                    Repeater {
                        model: page.islands ? 3 : 5
                        Rectangle {
                            required property int index
                            width: page.islands ? (desktop.vertical ? 16 : index === 1 ? 78 : 28) : 9
                            height: page.islands ? (desktop.vertical ? index === 1 ? 42 : 18 : 20) : 9
                            radius: page.islands ? 7 : 3
                            x: desktop.vertical ? (previewBar.width - width) / 2
                               : page.islands ? (index === 0 ? 5 : index === 1 ? (previewBar.width - width) / 2 : previewBar.width - width - 5) : previewBar.width / 2 - 36 + index * 15
                            y: desktop.vertical ? (page.islands ? (index === 0 ? 3 : index === 1 ? (previewBar.height - height) / 2 : previewBar.height - height - 3) : previewBar.height / 2 - 36 + index * 15)
                               : (previewBar.height - height) / 2
                            color: page.islands ? "#a0dce1ed" : index === 0 ? page.accent : "#dce1ed"
                            Behavior on radius { NumberAnimation { duration: 180 } }
                            Rectangle {
                                anchors.centerIn: parent; width: 6; height: 6; radius: 2
                                visible: page.islands; color: index === 0 ? page.accent : "white"
                            }
                        }
                    }
                }
                HoverHandler { id: previewHover }
                Caption {
                    anchors.centerIn: parent
                    text: "Предпросмотр"
                    font.pixelSize: 10; opacity: 0.5
                }
            }

            Rectangle {
                Layout.fillWidth: true; implicitHeight: 84
                radius: 13; color: "#12ffffff"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8
                    Caption { text: "Расположение панели" }
                    RowLayout {
                        spacing: 5
                        Repeater {
                            model: [ {label: "Сверху", key: "top"}, {label: "Снизу", key: "bottom"},
                                     {label: "Слева", key: "left"}, {label: "Справа", key: "right"} ]
                            Choice {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.label
                                enabled: page.sidesAvailable || modelData.key === "top" || modelData.key === "bottom"
                                opacity: enabled ? 1 : 0.35
                                selected: page.edge === modelData.key
                                onClicked: {
                                    page.edge = modelData.key
                                    if (page.edge !== "bottom") page.autoHide = false
                                    page.scheduleApply()
                                }
                                Accessible.name: "Расположение: " + text
                            }
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 84
                radius: 13; color: "#12ffffff"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8
                    Caption { text: "Вид панели" }
                    RowLayout {
                        spacing: 6
                        Choice {
                            Layout.fillWidth: true; text: "Слитный"
                            objectName: "taskbarStyleAttached"
                            selected: page.panelStyle === "attached"
                            onClicked: page.selectStyle("attached")
                        }
                        Choice {
                            Layout.fillWidth: true; text: "Отдельный"
                            objectName: "taskbarStyleDetached"
                            selected: page.panelStyle === "detached"
                            onClicked: page.selectStyle("detached")
                        }
                        Choice {
                            objectName: "taskbarStyleIslands"
                            Layout.fillWidth: true; text: "Островки"
                            selected: page.panelStyle === "islands"
                            enabled: page.widthAvailable
                            opacity: enabled ? 1 : 0.35
                            onClicked: page.selectStyle("islands")
                            Accessible.name: "Вид панели: Островки"
                        }
                    }
                }
            }
            Rectangle {
                objectName: "taskbarWidthSection"
                Layout.fillWidth: true
                implicitHeight: page.detached ? 84 : 0
                opacity: page.detached ? 1 : 0
                visible: implicitHeight > 0
                clip: true; radius: 13; color: "#12ffffff"
                Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
                Behavior on opacity { NumberAnimation { duration: 180 } }
                ColumnLayout {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 10
                    height: 64; spacing: 4
                    RowLayout {
                        Caption { text: page.vertical ? "Длина панели" : "Ширина панели"; Layout.fillWidth: true }
                        Caption { text: Math.round(page.widthPercent) + "%"; opacity: 0.8 }
                    }
                    Slider {
                        id: widthSlider
                        objectName: "taskbarWidth"
                        Layout.fillWidth: true; implicitHeight: 30
                        from: 40; to: 100; stepSize: 1; value: page.widthPercent
                        enabled: page.widthAvailable && !page.autoHide
                        opacity: enabled ? 1 : 0.35
                        onMoved: { page.widthPercent = value; page.scheduleApply() }
                        Accessible.name: page.vertical ? "Длина панели" : "Ширина панели"
                        background: Rectangle {
                            x: widthSlider.leftPadding
                            y: (widthSlider.height - height) / 2
                            width: widthSlider.availableWidth; height: 6; radius: 3
                            color: "#28ffffff"
                            Rectangle {
                                width: widthSlider.visualPosition * parent.width
                                height: parent.height; radius: parent.radius; color: page.accent
                            }
                        }
                        handle: Rectangle {
                            x: widthSlider.leftPadding + widthSlider.visualPosition * (widthSlider.availableWidth - width)
                            y: (widthSlider.height - height) / 2
                            width: 18; height: 18; radius: 9; color: "white"
                            scale: widthSlider.pressed || widthSlider.hovered ? 1.16 : 1
                            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }
            }
            Rectangle {
                objectName: "taskbarThicknessSection"
                Layout.fillWidth: true
                implicitHeight: page.detached ? 84 : 0
                opacity: page.detached ? 1 : 0
                visible: implicitHeight > 0
                clip: true; radius: 13; color: "#12ffffff"
                Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.InOutCubic } }
                Behavior on opacity { NumberAnimation { duration: 180 } }
                ColumnLayout {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.top: parent.top; anchors.margins: 10; height: 64; spacing: 4
                    RowLayout {
                        Caption { text: page.vertical ? "Толщина панели" : "Высота панели"; Layout.fillWidth: true }
                        Caption { text: page.thickness + " px"; opacity: 0.8 }
                    }
                    Slider {
                        id: thicknessSlider
                        objectName: "taskbarThickness"
                        Layout.fillWidth: true; implicitHeight: 30
                        from: page.islands ? 64 : 48; to: 96; stepSize: 2; value: page.thickness
                        enabled: page.widthAvailable && !page.autoHide
                        opacity: enabled ? 1 : 0.35
                        onMoved: { page.thickness = Math.round(value); page.scheduleApply() }
                        Accessible.name: "Толщина панели"
                        background: Rectangle {
                            x: thicknessSlider.leftPadding
                            y: (thicknessSlider.height - height) / 2
                            width: thicknessSlider.availableWidth; height: 6; radius: 3; color: "#28ffffff"
                            Rectangle { width: thicknessSlider.visualPosition * parent.width; height: parent.height; radius: 3; color: page.accent }
                        }
                        handle: Rectangle {
                            x: thicknessSlider.leftPadding + thicknessSlider.visualPosition * (thicknessSlider.availableWidth - width)
                            y: (thicknessSlider.height - height) / 2
                            width: 18; height: 18; radius: 9; color: "white"
                            scale: thicknessSlider.pressed || thicknessSlider.hovered ? 1.16 : 1
                            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 83
                radius: 13; color: "#12ffffff"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 4
                    RowLayout {
                        Caption { text: "Прозрачность панели"; Layout.fillWidth: true }
                        Caption { text: Math.round(page.transparency) + "%"; opacity: 0.8 }
                    }
                    Slider {
                        id: transparencySlider
                        Layout.fillWidth: true
                        from: 0; to: 100; value: page.transparency; stepSize: 1
                        onMoved: { page.transparency = value; page.scheduleApply() }
                        Accessible.name: "Прозрачность панели"
                        implicitHeight: 30
                        background: Rectangle {
                            x: transparencySlider.leftPadding
                            y: (transparencySlider.height - height) / 2
                            width: transparencySlider.availableWidth; height: 6; radius: 3
                            color: "#28ffffff"
                            Rectangle {
                                width: transparencySlider.visualPosition * parent.width
                                height: parent.height; radius: parent.radius
                                color: page.accent
                            }
                        }
                        handle: Rectangle {
                            x: transparencySlider.leftPadding + transparencySlider.visualPosition * (transparencySlider.availableWidth - width)
                            y: (transparencySlider.height - height) / 2
                            width: 18; height: 18; radius: 9
                            color: "white"
                            scale: transparencySlider.pressed || transparencySlider.hovered ? 1.16 : 1
                            Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 64
                radius: 13; color: "#12ffffff"
                RowLayout {
                    anchors.fill: parent; anchors.margins: 10
                    ColumnLayout {
                        spacing: 3; Layout.fillWidth: true
                        Caption { text: "Автоскрытие" }
                        Caption { text: page.islands ? "Недоступно для вида «Островки»" : page.edge === "bottom" ? "Полная панель · стандартная толщина" : "Доступно при расположении снизу"; opacity: 0.6; font.pixelSize: 10 }
                    }
                    Toggle {
                        objectName: "taskbarAutoHide"
                        checked: page.autoHide
                        enabled: page.edge === "bottom" && !page.islands
                        opacity: enabled ? 1 : 0.4
                        onToggled: {
                            page.autoHide = checked
                            if (checked) { page.widthPercent = 100; page.thickness = 48; page.islands = false }
                            page.scheduleApply()
                        }
                        Accessible.name: "Автоскрытие панели"
                    }
                }
            }
            Choice {
                text: "Вернуть исходный вид"
                Layout.fillWidth: true
                enabled: page.system && !page.system.pending
                onClicked: {
                    liveApply.stop()
                    page.applyQueued = false
                    page.system.resetAppearance()
                }
            }
            Caption {
                Layout.fillWidth: true
                text: page.system ? page.system.status : ""
                visible: text.length > 0
                wrapMode: Text.WordWrap; opacity: 0.85; font.pixelSize: 11
            }
            Caption {
                Layout.fillWidth: true
                text: "Изменения применяются автоматически. При выходе оформление вернётся. Прозрачность влияет также на значки."
                wrapMode: Text.WordWrap; opacity: 0.7; font.pixelSize: 11
            }
            Item { implicitHeight: 8 }
        }
    }
}
