import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "TaskbarLayout.js" as LayoutModel

Rectangle {
    id: editor
    objectName: "taskbarConfiguration"
    property var scrollTarget
    property real sceneX:0
    property real sceneY:0
    property var system
    property var monitors: system && system.layoutMonitors !== undefined ? system.layoutMonitors : [{id:"primary", label:"Основной монитор", primary:true, width:2560}, {id:"secondary", label:"Второй монитор (модель)", primary:false, width:2560}]
    property int monitorIndex: 0
    readonly property var monitor: monitors.length ? monitors[Math.min(monitorIndex, monitors.length - 1)] : null
    property var configurations: ({})
    property var configuration: LayoutModel.defaults(true)
    property string dragId: ""
    property string hoverZone: ""
    property int hoverIndex: -1
    property real pointerX: 0
    property real pointerY: 0
    property string feedback: "Черновик. Перестановки здесь не применяются к Windows автоматически."
    readonly property bool nativeAvailable: !!(system && system.componentPlacementAvailable && monitor && monitor.primary)
    readonly property string applyScenario: LayoutModel.taskViewPlacement(configuration)
    readonly property color accent: "#e7131b"
    implicitHeight: body.implicitHeight + 24
    radius: 13; color: "#12ffffff"
    function loadMonitor() {
        cancelDrag()
        if (!monitor) return
        var saved = configurations[monitor.id]
        if (!saved && system && system.componentDraft) saved = system.componentDraft(monitor.id)
        if (saved) saved = LayoutModel.clone(saved) // Normalize QVariantList sequences into JS arrays.
        configuration = LayoutModel.valid(saved) ? saved : LayoutModel.defaults(monitor.primary)
        feedback = "Черновик выбранного монитора загружен. Открытие не применяет его к Windows."
    }
    Component.onCompleted: loadMonitor()
    onMonitorChanged: loadMonitor()
    function commit(next) {
        if (!LayoutModel.valid(next) || !monitor) return false
        configuration = next
        var all = Object.assign({}, configurations)
        all[monitor.id] = next; configurations = all
        var saved = !system || !system.saveComponentDraft || system.saveComponentDraft(monitor.id,next)
        feedback = saved ? "Черновик этого монитора сохранён. Windows не изменена; примените Task View отдельно." : "Черновик изменён только в этом сеансе: сохранить его не удалось."
        return true
    }
    function moveComponent(id, zone, index) { return commit(LayoutModel.move(configuration, id, zone, index)) }
    function cancelDrag() { dragId = ""; hoverZone = ""; hoverIndex = -1 }
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
        if (id && zone) moveComponent(id, zone, index)
    }
    function resetLayout() {
        cancelDrag()
        if (monitor && monitor.primary && system && system.resetComponentLayout && !system.resetComponentLayout()) { feedback = "Восстановление Windows не подтверждено. Черновик сохранён."; return }
        if (monitor) commit(LayoutModel.defaults(monitor.primary))
        feedback = monitor && monitor.primary ? "Черновик сброшен к базовому расположению каталога. Смещение Task View снято." : "Черновик дополнительного монитора сброшен. Основная панель и её черновик не изменены."
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
        height: 52; radius:9
        color: mouse.pressed ? "#40ffffff" : mouse.containsMouse ? "#28ffffff" : "#15ffffff"
        border.width: 1; border.color: "#20ffffff"
        opacity: editor.dragId === componentId ? 0 : 1
        Behavior on color { ColorAnimation { duration:120 } }
        Column {
            anchors.left:parent.left; anchors.right:parent.right; anchors.margins:9; anchors.verticalCenter:parent.verticalCenter; spacing:2
            Label { width:parent.width; text:chip.info.label; font.pixelSize:11; maximumLineCount:1; elide:Text.ElideRight }
            Label { width:parent.width; font.pixelSize:9; opacity:0.6
                text: chip.info.native ? (editor.nativeAvailable ? "Task View · применение отдельно" : "Task View · нужна совместимость") : (chip.knownHere ? "Пока не поддерживается" : "Не обнаружен здесь · не поддерживается")
            }
        }
        ToolTip.visible: mouse.containsMouse && !mouse.pressed
        ToolTip.text: info.label + "\n" + (chip.knownHere ? "Обнаружен на этом мониторе в каталоге исследования" : "На этом мониторе не обнаружен; наличие не подтверждено") + "\n" + (info.native ? "Перенос внутри исходного контейнера; логический порядок не меняется" : "Применение к Windows пока не поддерживается")
        Accessible.name: info.label
        MouseArea {
            id:mouse; anchors.fill:parent; hoverEnabled:true; preventStealing:true
            property real pressX:0; property real pressY:0
            onPressed: function(e) { var p = mapToItem(editor,e.x,e.y); pressX=p.x; pressY=p.y }
            onPositionChanged: function(e) {
                if (!pressed) return
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
            Label { text:"Конфигурация панели"; font.pixelSize:16; font.weight:Font.DemiBold; Layout.fillWidth:true }
            ActionButton { text:"Сброс"; objectName:"configurationReset"; onClicked:editor.resetLayout() }
        }
        ComboBox {
            objectName:"configurationMonitor"; Layout.fillWidth:true
            palette.button:"#30343c"; palette.base:"#30343c"; palette.text:"white"; palette.buttonText:"white"
            model:editor.monitors; textRole:"label"; currentIndex:editor.monitorIndex
            onActivated: function(index) { editor.monitorIndex=index }
        }
        Label { Layout.fillWidth:true; visible:editor.monitor && !editor.monitor.primary; text:"Дополнительный монитор: независимый черновик состава и порядка. Применение к Windows пока не поддерживается."; opacity:0.75; font.pixelSize:11 }
        Label { text:"Состав для выбранного монитора · перетащите компоненты между каталогом и областями. Черновики мониторов сохраняются отдельно; автоматически не применяются."; opacity:0.7; Layout.fillWidth:true }
        Rectangle {
            id:availableArea; objectName:"configurationAvailable"; Layout.fillWidth:true; implicitHeight:84
            color:editor.hoverZone === "available" ? "#30e7131b" : "#10ffffff"; radius:9
            border.color:editor.hoverZone === "available" ? editor.accent : "#15ffffff"
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
                    radius:10; color:editor.hoverZone === zoneKey ? "#20e7131b" : "#0dffffff"
                    border.color:editor.hoverZone === zoneKey ? editor.accent : "#20ffffff"
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
                                    height:!slot.source && editor.hoverZone === zone.zoneKey && editor.hoverIndex === slot.insertionIndex ? 58 : 0
                                    color:"transparent"; clip:true
                                    Behavior on height { NumberAnimation { duration:110 } }
                                    Rectangle { anchors.centerIn:parent; width:parent.width; height:3; radius:1; color:editor.accent }
                                }
                                Item {
                                    width:parent.width; height:slot.source ? 0 : 58
                                    Chip { componentId:slot.modelData; width:parent.width }
                                }
                            }
                        }
                        Rectangle {
                            width:parent.width
                            height:editor.hoverZone === zone.zoneKey && editor.hoverIndex === editor.configuration[zone.zoneKey].filter(v => v !== editor.dragId).length ? 58 : 0
                            color:"transparent"; clip:true
                            Behavior on height { NumberAnimation { duration:110 } }
                            Rectangle { anchors.centerIn:parent; width:parent.width; height:3; radius:1; color:editor.accent }
                        }
                        Label { visible:editor.configuration[zone.zoneKey].length === 0 && editor.hoverZone !== zone.zoneKey; width:parent.width; text:"Перетащите сюда"; opacity:0.4 }
                    }
                }
            }
        }
        RowLayout {
            Label { text:"Расстояние в черновике"; Layout.fillWidth:true }
            SpinBox { palette.button:"#30343c"; palette.base:"#30343c"; palette.text:"white"; palette.buttonText:"white"; objectName:"configurationGap"; from:0; to:96; value:editor.configuration.gap; editable:true
                onValueModified: { var next=LayoutModel.clone(editor.configuration); next.gap=value; editor.commit(next) }
            }
            Label { text:"DIP"; opacity:0.6 }
        }
        Label { Layout.fillWidth:true; text:"Применение: Task View сразу после «Виджетов» слева, основной монитор, исходная нижняя панель 48 DIP / масштаб 100%. Другие позиции пока не поддерживаются. Остальные карточки — черновик; логический порядок и контейнеры Explorer не меняются."; opacity:0.65; font.pixelSize:11 }
        RowLayout {
            ActionButton {
                objectName:"configurationApply"; text:"Применить только Task View"; enabled:editor.nativeAvailable && editor.applyScenario !== "unsupported" && !editor.dragId
                onClicked: {
                    var ok = editor.applyScenario === "reset" ? editor.system.resetComponentLayout() : editor.system.placeTaskViewAfterWidgets(editor.monitor.id,editor.configuration.gap)
                    editor.feedback = ok ? (editor.applyScenario === "reset" ? "Смещение Task View снято. Возврат в каталог не скрывает кнопку Windows." : "Применена только Task View; остальные изменения черновика не применены.") : "Применение не подтверждено. Подробности в статусе Windows ниже."
                }
            }
            ActionButton { text:"Вернуть Task View"; visible:editor.monitor && editor.monitor.primary; onClicked: { if (editor.system && editor.system.resetComponentLayout) editor.system.resetComponentLayout() } }
        }
        Label { Layout.fillWidth:true; visible:editor.applyScenario === "unsupported"; text:"Эта позиция Task View пока не поддерживается. Проверяемый сценарий: «Виджеты», затем «Представление задач» в области «Слева»."; font.pixelSize:11; opacity:0.7 }
        Label { objectName:"configurationFeedback"; Layout.fillWidth:true; text:editor.feedback; opacity:0.85; font.pixelSize:11 }
        Label { Layout.fillWidth:true; visible:!!(editor.system && editor.system.status); text:editor.system ? editor.system.status : ""; opacity:0.7; font.pixelSize:11 }
    }
    Rectangle {
        z:20; visible:editor.dragId !== ""; x:editor.pointerX-80; y:editor.pointerY-24; width:175; height:48; radius:9
        color:"#ef30343c"; border.color:editor.accent
        Label { anchors.fill:parent; anchors.margins:8; text:editor.dragId ? LayoutModel.component(editor.dragId).label : ""; verticalAlignment:Text.AlignVCenter }
    }
    Shortcut { sequence:"Escape"; enabled:editor.dragId !== ""; onActivated:editor.cancelDrag() }
}
