import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "TaskbarLayout.js" as LayoutModel

Rectangle {
    id: editor
    objectName: "taskbarConfiguration"
    property var scrollTarget
    property real sceneX:0
    property real sceneY:0
    property var system
    property var monitors: system && system.layoutMonitors !== undefined ? system.layoutMonitors : []
    property int monitorIndex: 0
    readonly property var monitor: monitors.length ? monitors[Math.min(monitorIndex, monitors.length - 1)] : null
    property var configuration: LayoutModel.defaults(true)
    property var queuedConfiguration: null
    property string queuedMonitor: ""
    property string loadedMonitor: ""
    property bool applying: false
    readonly property bool canSubmit: visible && enabled && (!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
    onCanSubmitChanged: if (!canSubmit) { cancelDrag(); discardQueuedApply() }
    property bool dragCancelled: false
    property string dragId: ""
    property string hoverZone: ""
    property int hoverIndex: -1
    readonly property bool validDrop: dragId !== "" && hoverZone !== "" && supported(LayoutModel.move(configuration, dragId, hoverZone, hoverIndex))
    property real pointerX: 0
    property real pointerY: 0
    property string feedback: ""
    readonly property bool nativeAvailable: !!(system && system.componentPlacementAvailable && monitor && monitor.primary)
    readonly property string applyScenario: LayoutModel.taskViewPlacement(configuration)
    readonly property color accent: "#e7131b"
    implicitHeight: body.implicitHeight + 24
    radius: 16; color: "#12ffffff"
    function currentConfiguration() {
        var live = system && system.activeComponentMonitor === (monitor ? monitor.id : "")
            ? LayoutModel.clone(system.activeComponentConfiguration) : null
        return LayoutModel.valid(live) ? live : LayoutModel.defaults(monitor ? monitor.primary : true)
    }
    function loadMonitor() {
        if (!monitor || loadedMonitor === monitor.id) return
        cancelDrag()
        // A queued drop belongs to the monitor on which it was made. Switching
        // screens cancels it before the delayed apply can change another panel.
        discardQueuedApply()
        loadedMonitor = monitor.id
        configuration = currentConfiguration()
        feedback = monitor.primary ? "" : "Перемещение на этом мониторе пока недоступно."
    }
    Component.onCompleted: loadMonitor()
    onMonitorChanged: loadMonitor()
    function supported(next) {
        if (!nativeAvailable || !LayoutModel.valid(next)) return false
        var expected = LayoutModel.move(LayoutModel.defaults(true), "taskview", "left", 1)
        return ["available", "left", "center", "right"].every(z => JSON.stringify(next[z]) === JSON.stringify(expected[z]))
    }
    function commit(next) {
        next = LayoutModel.clone(next)
        if (!canSubmit || !supported(next) || !monitor || applying) {
            feedback = "Доступно перемещение Task View после Виджетов слева."
            return false
        }
        queuedConfiguration = next
        queuedMonitor = monitor.id
        feedback = ""
        autoApply.restart()
        return true
    }
    function moveComponent(id, zone, index) {
        if (id !== "taskview") return false
        return commit(LayoutModel.move(configuration, id, zone, index))
    }
    Timer {
        id: autoApply; interval: 180; repeat: false
        onTriggered: {
            if (!editor.queuedConfiguration) return
            if (!editor.canSubmit) { editor.discardQueuedApply(); return }
            if (editor.system.pending) return
            var next = editor.queuedConfiguration, target = editor.queuedMonitor
            editor.queuedConfiguration = null
            editor.queuedMonitor = ""
            editor.applying = true
            var ok = editor.system.applyComponentConfiguration(target, next)
            editor.applying = false
            editor.configuration = editor.currentConfiguration()
            editor.feedback = ok ? (editor.system.status || "") : "Windows не подтвердила перемещение. Проверьте панель задач."
        }
    }
    Connections {
        target: editor.system
        function onChanged() {
            if (editor.queuedConfiguration && !editor.system.pending && !editor.applying) autoApply.restart()
            if (!editor.applying && !editor.dragId) editor.configuration = editor.currentConfiguration()
        }
    }
    function discardQueuedApply() {
        if (autoApply) autoApply.stop()
        queuedConfiguration = null
        queuedMonitor = ""
    }
    function cancelDrag() { dragCancelled = true; dragId = ""; hoverZone = ""; hoverIndex = -1 }
    function updatePointer(x, y) {
        var scene = editor.mapToItem(null,x,y); sceneX=scene.x; sceneY=scene.y
        pointerX = x; pointerY = y; hoverZone = ""; hoverIndex = -1
        var p = availableArea.mapFromItem(editor, x, y)
        if (p.x >= 0 && p.x < availableArea.width && p.y >= 0 && p.y < availableArea.height) {
            hoverZone = "available"; hoverIndex = configuration.available.filter(v => v !== dragId).length; return
        }
        for (var i = 0; i < zones.count; ++i) {
            var zone = zones.itemAt(i)
            p = zone.mapFromItem(editor, x, y)
            if (p.x >= 0 && p.x < zone.width && p.y >= 0 && p.y < zone.height) {
                hoverZone = zone.zoneKey
                var count = configuration[hoverZone].filter(v => v !== dragId).length
                hoverIndex = Math.max(0, Math.min(count, Math.floor((p.y - 42 + 28) / 58)))
                return
            }
        }
    }
    function finishDrag() {
        var id = dragId, zone = hoverZone, index = hoverIndex
        cancelDrag()
        if (id && zone) {
            var next = LayoutModel.move(configuration, id, zone, index)
            if (supported(next)) commit(next)
            else feedback = "Доступно перемещение Task View после Виджетов слева."
        }
    }
    function resetLayout() {
        cancelDrag()
        discardQueuedApply()
        if (!monitor || !system) return
        applying = true
        var ok = system.restoreComponentConfiguration(monitor.id)
        applying = false
        configuration = currentConfiguration()
        feedback = ok ? "" : (system.status || "Не удалось подтвердить восстановление. Повторите попытку.")
    }
    Timer {
        interval:40; repeat:true; running:editor.dragId !== "" && !!editor.scrollTarget
        onTriggered: {
            var p=editor.scrollTarget.mapFromItem(null,editor.sceneX,editor.sceneY)
            var delta=p.y < 40 ? -12 : p.y > editor.scrollTarget.height-40 ? 12 : 0
            if (delta) {
                editor.scrollTarget.contentY=Math.max(0,Math.min(editor.scrollTarget.contentHeight-editor.scrollTarget.height,editor.scrollTarget.contentY+delta))
                var local=editor.mapFromItem(null,editor.sceneX,editor.sceneY)
                editor.updatePointer(local.x,local.y)
            }
        }
    }
    component ActionButton: Button {
        id:action
        font.family:"Segoe UI"; font.pixelSize:11; implicitHeight:34; hoverEnabled:true
        background:Rectangle { radius:9; color:action.down ? "#40ffffff" : action.hovered ? "#30ffffff" : "#18ffffff" }
        contentItem:Text { text:action.text; font:action.font; color:"white"; opacity:action.enabled ? 1 : 0.35; horizontalAlignment:Text.AlignHCenter; verticalAlignment:Text.AlignVCenter }
    }
    component Label: Text {
        color:"white"; font.family:"Segoe UI"; font.pixelSize:12
        wrapMode: Text.WordWrap
    }
    component Chip: Rectangle {
        id: chip
        required property string componentId
        property var info: LayoutModel.component(componentId)
        property bool inCatalog: false
        readonly property bool knownHere: !!(info.seen && editor.monitor && (editor.monitor.primary || info.secondary))
        objectName: "component_" + componentId
        readonly property bool movable: componentId === "taskview" && editor.nativeAvailable && !editor.applying
        height: 52; radius:10
        color: mouse.pressed ? "#40ffffff" : mouse.containsMouse ? "#28ffffff" : "#15ffffff"
        border.width: 1; border.color: "#20ffffff"
        opacity: editor.dragId === componentId ? 0 : 1
        Behavior on color { ColorAnimation { duration:120 } }
        Column {
            anchors.left:parent.left; anchors.leftMargin:36; anchors.right:parent.right; anchors.rightMargin:9; anchors.verticalCenter:parent.verticalCenter; spacing:2
            Label { width:parent.width; text:chip.info.label; font.pixelSize:11; maximumLineCount:1; elide:Text.ElideRight }
            Label { width:parent.width; font.pixelSize:9; opacity:0.6
                text: chip.movable ? "После Виджетов слева" : ""
                visible:chip.movable
            }
        }
        ToolTip.visible: mouse.containsMouse && !mouse.pressed
        Text {
            x:10; anchors.verticalCenter:parent.verticalCenter
            visible:chip.componentId !== "start"
            font.family:"Segoe MDL2 Assets"; font.pixelSize:18; color:"white"; opacity:chip.movable ? 1 : 0.45
            text: ({widgets:"\uE80A",start:"\uE782",search:"\uE721",taskview:"\uE7C4",apps:"\uE71D",clock:"\uE787",volume:"\uE767",network:"\uE701",input:"\uE8D2"})[chip.componentId] || "\uE713"
        }
        Item {
            x:10; width:18; height:18; anchors.verticalCenter:parent.verticalCenter
            visible:chip.componentId === "start"; opacity:0.45
            Repeater {
                model:4
                Rectangle { required property int index; x:(index % 2)*9; y:Math.floor(index/2)*9; width:7; height:7; color:"white" }
            }
        }
        ToolTip.text: chip.movable ? "Task View → Слева, после Виджетов" : LayoutModel.movementBlocker(componentId,
            !!(editor.monitor && editor.monitor.primary), !!(editor.system && editor.system.componentPlacementAvailable))
        Accessible.name: info.label
        MouseArea {
            id:mouse; anchors.fill:parent; hoverEnabled:true; preventStealing:true
            cursorShape:chip.movable ? Qt.OpenHandCursor : Qt.ArrowCursor
            property real pressX:0; property real pressY:0
            onPressed: function(e) { editor.dragCancelled = false; var p = mapToItem(editor,e.x,e.y); pressX=p.x; pressY=p.y }
            onPositionChanged: function(e) {
                if (!pressed || !chip.movable || editor.dragCancelled) return
                var p = mapToItem(editor,e.x,e.y)
                if (!editor.dragId && Math.hypot(p.x-pressX,p.y-pressY) > 6) editor.dragId=chip.componentId
                if (editor.dragId) editor.updatePointer(p.x,p.y)
            }
            onReleased: editor.finishDrag()
            onCanceled: editor.cancelDrag()
        }
    }
    ColumnLayout {
        id:body; anchors.left:parent.left; anchors.right:parent.right; anchors.top:parent.top; anchors.margins:12; spacing:10
        RowLayout {
            Layout.fillWidth:true
            Label { text:"Конфигурация панели"; font.pixelSize:22; font.weight:Font.DemiBold; Layout.fillWidth:true }
            ActionButton { text:"Восстановить"; objectName:"configurationReset"; enabled:!editor.applying && !!editor.monitor; onClicked:editor.resetLayout() }
        }
        RowLayout {
            objectName:"configurationMonitor"; Layout.fillWidth:true; spacing:6
            Repeater {
                model:editor.monitors
                ActionButton {
                    required property var modelData
                    required property int index
                    text:"Монитор " + (index + 1)
                    objectName:"configurationMonitor_" + index
                    enabled:!editor.applying && !editor.queuedConfiguration
                    background:Rectangle {
                        radius:10; color:index === editor.monitorIndex ? editor.accent : "#18ffffff"
                        Behavior on color { ColorAnimation { duration:160 } }
                    }
                    onClicked:editor.monitorIndex=index
                    ToolTip.visible:hovered; ToolTip.text:modelData.label
                }
            }
            Item { Layout.fillWidth:true }
        }
        Label { text:"Компоненты"; opacity:0.65; font.pixelSize:11 }
        Rectangle {
            id:availableArea; objectName:"configurationAvailable"; Layout.fillWidth:true; implicitHeight:84
            color:editor.validDrop && editor.hoverZone === "available" ? "#30e7131b" : "#10ffffff"; radius:9
            border.color:editor.validDrop && editor.hoverZone === "available" ? editor.accent : "#15ffffff"
            Flickable {
                anchors.fill:parent; anchors.margins:8; clip:true; contentWidth:catalogRow.width; contentHeight:height
                flickableDirection:Flickable.HorizontalFlick
                Row {
                    id:catalogRow; spacing:6
                    Repeater {
                        model:editor.configuration.available
                        Chip { required property string modelData; componentId:modelData; width:168; inCatalog:true }
                    }
                    Label { visible:editor.configuration.available.length === 0; text:"Все компоненты размещены. Перетащите сюда для возврата."; width:300; height:52; verticalAlignment:Text.AlignVCenter }
                }
                ScrollBar.horizontal: ScrollBar { }
            }
        }
        RowLayout {
            Layout.fillWidth:true; spacing:8
            Repeater {
                id:zones
                model:[{key:"left",label:"Слева"},{key:"center",label:"По центру"},{key:"right",label:"Справа"}]
                Rectangle {
                    id:zone
                    required property var modelData
                    property string zoneKey:modelData.key
                    objectName:"configuration_" + zoneKey
                    Layout.fillWidth:true; Layout.alignment:Qt.AlignTop
                    implicitHeight:Math.max(160,cards.implicitHeight+54)
                    radius:10; color:editor.validDrop && editor.hoverZone === zoneKey ? "#20e7131b" : "#0dffffff"
                    border.color:editor.validDrop && editor.hoverZone === zoneKey ? editor.accent : "#20ffffff"
                    Label { x:10; y:12; text:zone.modelData.label; font.weight:Font.DemiBold }
                    Column {
                        id:cards; x:8; y:42; width:parent.width-16
                        move:Transition { NumberAnimation { properties:"y"; duration:140; easing.type:Easing.OutCubic } }
                        Repeater {
                            id:cardRepeater; model:editor.configuration[zone.zoneKey]
                            Column {
                                id:slot
                                required property string modelData
                                required property int index
                                readonly property int sourceIndex: editor.configuration[zone.zoneKey].indexOf(editor.dragId)
                                readonly property int insertionIndex:index - (sourceIndex >= 0 && sourceIndex < index ? 1 : 0)
                                readonly property bool source:editor.dragId === modelData
                                width:cards.width
                                Rectangle {
                                    width:parent.width
                                    height:!slot.source && editor.validDrop && editor.hoverZone === zone.zoneKey && editor.hoverIndex === slot.insertionIndex ? 58 : 0
                                    color:"transparent"; clip:true
                                    Behavior on height { NumberAnimation { duration:110 } }
                                    Rectangle { anchors.centerIn:parent; width:parent.width; height:3; radius:1; color:editor.accent }
                                }
                                Item {
                                    width:parent.width; height:slot.source ? 0 : 58
                                    Behavior on height { NumberAnimation { duration:110; easing.type:Easing.OutCubic } }
                                    Chip { componentId:slot.modelData; width:parent.width }
                                }
                            }
                        }
                        Rectangle {
                            width:parent.width
                            height:editor.validDrop && editor.hoverZone === zone.zoneKey && editor.hoverIndex === editor.configuration[zone.zoneKey].filter(v => v !== editor.dragId).length ? 58 : 0
                            color:"transparent"; clip:true
                            Behavior on height { NumberAnimation { duration:110 } }
                            Rectangle { anchors.centerIn:parent; width:parent.width; height:3; radius:1; color:editor.accent }
                        }
                        Label { visible:editor.configuration[zone.zoneKey].length === 0 && editor.hoverZone !== zone.zoneKey; width:parent.width; text:"Нет компонентов"; opacity:0.4 }
                    }
                }
            }
        }
        RowLayout {
            Label { text:"Расстояние"; Layout.fillWidth:true }
            SpinBox { palette.button:"#30343c"; palette.base:"#30343c"; palette.text:"white"; palette.buttonText:"white"; objectName:"configurationGap"; enabled:editor.nativeAvailable && editor.applyScenario === "after-widgets" && !editor.applying; from:0; to:96; value:editor.configuration.gap; editable:true
                onValueModified: { var next=LayoutModel.clone(editor.queuedConfiguration || editor.configuration); next.gap=value; editor.commit(next) }
            }
            Label { text:"DIP"; opacity:0.6 }
        }
        Label {
            objectName:"configurationFeedback"; Layout.fillWidth:true
            visible:text.length > 0
            text:editor.feedback; opacity:0.85; font.pixelSize:11
        }
    }
    Rectangle {
        z:20; visible:editor.dragId !== ""; x:editor.pointerX-80; y:editor.pointerY-24; width:175; height:48; radius:9
        color:"#ef30343c"; border.color:editor.accent
        Label { anchors.fill:parent; anchors.margins:8; text:editor.dragId ? LayoutModel.component(editor.dragId).label : ""; verticalAlignment:Text.AlignVCenter }
    }
    Shortcut { sequence:"Escape"; enabled:editor.canSubmit && (editor.dragId !== "" || !!editor.queuedConfiguration); onActivated: { editor.cancelDrag(); editor.discardQueuedApply() } }
}
