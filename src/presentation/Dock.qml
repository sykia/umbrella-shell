import QtQuick
import QtQuick.Controls

Item {
    id: root
    property bool preview: false
    property string screenName: typeof dockScreen !== "undefined" ? dockScreen : ""
    readonly property int appCount: apps.count
    property var options: dockSystem.config
    property bool concealed: false
    property bool backdropAvailable: false
    readonly property bool vertical: options.edge === "left" || options.edge === "right"
    readonly property bool dark: options.theme === "dark" || (options.theme === "auto" && dockSystem.theme.dark)
    readonly property bool contrast: dockSystem.theme.contrast
    readonly property bool animate: options.animations && dockSystem.theme.animations
    readonly property int duration: animate ? Math.round(18000 / options.speed) : 0
    property real entrance:0
    property real presence:concealed ? 0 : 1
    Component.onCompleted:entrance=1
    Behavior on entrance { NumberAnimation { duration:root.duration } }
    Behavior on presence { NumberAnimation { duration:root.duration } }
    readonly property color foreground: contrast ? dockSystem.theme.foreground : dark ? "#f4f4f4" : "#171717"
    readonly property color surface: contrast ? dockSystem.theme.background : options.theme === "auto"
        ? Qt.tint(dark ? "#25272d" : "#f0f2f5", Qt.rgba(dockSystem.theme.accent.r,dockSystem.theme.accent.g,dockSystem.theme.accent.b,0.1))
        : dark ? "#25272d" : "#f0f2f5"
    property int hoverIndex: -100
    property int menuRow: -1
    property string menuIdentity: ""
    property var menuWindows: []
    property bool menuPinned: false
    readonly property bool menuOpen: appMenu.opened || brandMenu.opened
    function updateInteraction() { if (!preview) dockSystem.interaction(screenName, hover.hovered, menuOpen) }
    onMenuOpenChanged: updateInteraction()
    HoverHandler { id: hover; onHoveredChanged: root.updateInteraction() }
    Repeater {
        model: root.options.shadows && !root.contrast && !root.concealed ? 3 : 0
        Rectangle {
            required property int index
            anchors.centerIn: parent
            width: parent.width-index*2; height: parent.height-index*2
            radius: root.options.radius+3-index
            color: "#10000000"
        }
    }
    Rectangle {
        id: panel
        anchors.fill: parent
        anchors.margins: root.concealed ? 0 : 3
        radius: root.contrast ? 0 : root.options.radius
        color: root.concealed ? "transparent" : Qt.rgba(root.surface.r, root.surface.g, root.surface.b,
                  root.backdropAvailable && !root.contrast ? root.options.opacity / 100 : 1)
        border.color: root.contrast ? root.foreground : root.dark ? "#55ffffff" : "#33000000"
        border.width: root.concealed ? 0 : 1
        opacity: root.presence*root.entrance
        scale: root.concealed ? 0.96 : 1
        Behavior on color { ColorAnimation { duration: root.duration } }
        Behavior on scale { NumberAnimation { duration: root.duration; easing.type:Easing.OutCubic } }
        clip: true
        enabled: !root.concealed
        Item {
            id: brand
            width: root.options.iconSize + 12; height: width
            x: root.vertical ? (panel.width-width)/2 : 8
            y: root.vertical ? 8 : (panel.height-height)/2
            Image {
                anchors.centerIn: parent
                width: root.options.iconSize; height: width
                source: "qrc:/assets/umbrella.png"; sourceSize.width: 128; sourceSize.height: 128
                fillMode: Image.PreserveAspectFit
                scale: brandMouse.pressed ? 0.9 : brandMouse.containsMouse ? 1.08 : 1
                Behavior on scale { NumberAnimation { duration: root.duration } }
            }
            MouseArea {
                id: brandMouse; anchors.fill: parent; hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) { if (root.preview) return; if (mouse.button === Qt.RightButton) brandMenu.popup(); else dockSystem.showSettings() }
                Accessible.name: "Umbrella — открыть настройки"; Accessible.role: Accessible.Button
            }
            ToolTip {
                popupType:Popup.Window
                visible:brandMouse.containsMouse && !root.menuOpen && !root.concealed
                delay:700; text:"Umbrella"
                x:root.vertical ? (root.options.edge==="left" ? brand.width+8 : -width-8) : (brand.width-width)/2
                y:root.vertical ? (brand.height-height)/2 : root.options.edge==="top" ? brand.height+8 : -height-8
            }
        }
        Rectangle {
            x: root.vertical ? 12 : brand.x + brand.width + 4
            y: root.vertical ? brand.y + brand.height + 4 : 12
            width: root.vertical ? panel.width-24 : 1; height: root.vertical ? 1 : panel.height-24
            color: root.foreground; opacity: 0.18
        }
        ListView {
            id: apps
            orientation: root.vertical ? ListView.Vertical : ListView.Horizontal
            x: root.vertical ? 6 : brand.x + brand.width + 12
            y: root.vertical ? brand.y + brand.height + 12 : 6
            width: panel.width-x-6; height: panel.height-y-6
            clip: true; boundsBehavior: Flickable.StopAtBounds
            model: dockSystem
            spacing: root.options.spacing
            add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.duration } }
            remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: root.duration } }
            displaced: Transition { NumberAnimation { properties: "x,y"; duration: root.duration; easing.type: Easing.OutCubic } }
            delegate: Item {
                id: app
                required property int index
                required property string identity
                required property string appTitle
                required property string iconSource
                required property bool pinned
                required property int windowCount
                required property bool appActive
                required property var appWindows
                readonly property real proximity: Math.max(0, 1-Math.abs(root.hoverIndex-index)*0.45)
                readonly property real growth: root.animate ? root.options.magnification / 100 * proximity : 0
                width: root.vertical ? apps.width : root.options.iconSize + 10 + growth*root.options.iconSize
                height: root.vertical ? root.options.iconSize + 10 + growth*root.options.iconSize : apps.height
                Behavior on width { NumberAnimation { duration: root.duration } }
                Behavior on height { NumberAnimation { duration: root.duration } }
                Rectangle {
                    anchors.fill: parent; anchors.margins: 1; radius: 10
                    color: dockSystem.theme.accent
                    opacity: app.appActive ? 0.25 : appMouse.containsMouse ? 0.12 : 0
                    Behavior on opacity { NumberAnimation { duration: root.duration } }
                }
                Image {
                    id: icon; anchors.centerIn: parent
                    width: Math.min(root.options.iconSize*(1+app.growth),parent.width-8,parent.height-8); height: width
                    source: app.iconSource; sourceSize.width: 80; sourceSize.height: 80
                    fillMode: Image.PreserveAspectFit; cache: false
                    scale: appMouse.pressed ? 0.88 : 1
                    Behavior on scale { NumberAnimation { duration: root.duration } }
                }
                Text {
                    anchors.centerIn: parent; visible: icon.status !== Image.Ready
                    text: app.appTitle.charAt(0); color: root.foreground; font.pixelSize: 24
                }
                Rectangle {
                    width: app.appActive ? 15 : 5; height: 3; radius: 2
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
                    color: app.appActive ? dockSystem.theme.accent : root.foreground
                    opacity: root.options.indicators && app.windowCount > 0 ? 1 : 0
                    Behavior on width { NumberAnimation { duration: root.duration } }
                    Behavior on opacity { NumberAnimation { duration: root.duration } }
                }
                MouseArea {
                    id: appMouse; anchors.fill: parent; hoverEnabled: true
                    preventStealing:app.pinned
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    property real pressX: 0
                    property real pressY: 0
                    onEntered: {
                        root.hoverIndex = app.index
                        if(!root.preview && root.options.previews) {
                            const point=app.mapToItem(root,0,0)
                            dockSystem.requestPreview(app.identity,root.screenName,point.x,point.y,app.width,app.height)
                        }
                    }
                    onExited: {
                        if(root.hoverIndex === app.index) root.hoverIndex = -100
                        if(!root.preview) dockSystem.cancelPreview(app.identity)
                    }
                    onPressed: function(mouse) { pressX=mouse.x; pressY=mouse.y; if(!root.preview) dockSystem.dismissPreview() }
                    onReleased: function(mouse) {
                        if (root.preview) return
                        if (mouse.button !== Qt.LeftButton || !app.pinned) return
                        const p = mapToItem(apps.contentItem,mouse.x,mouse.y)
                        if (Math.abs(mouse.x-pressX)+Math.abs(mouse.y-pressY)>10) {
                            const target=apps.indexAt(p.x,p.y)
                            if (target>=0) dockSystem.movePin(app.index,target)
                        }
                    }
                    onClicked: function(mouse) {
                        if (root.preview) return
                        if (Math.abs(mouse.x-pressX)+Math.abs(mouse.y-pressY)>10) return
                        if (mouse.button===Qt.RightButton) {
                            root.menuRow=app.index; root.menuIdentity=app.identity; root.menuWindows=app.appWindows; root.menuPinned=app.pinned; appMenu.popup()
                        } else if (mouse.button===Qt.MiddleButton) dockSystem.launch(app.index)
                        else dockSystem.activate(app.index)
                    }
                    Accessible.name: app.appTitle; Accessible.role: Accessible.Button
                }
                ToolTip {
                    popupType:Popup.Window
                    visible:appMouse.containsMouse && !root.menuOpen && !appMouse.pressed && !root.concealed && !root.options.previews
                    delay:700; text:app.appTitle + (app.windowCount>1 ? " · " + app.windowCount + " окна" : "")
                    x:root.vertical ? (root.options.edge==="left" ? app.width+8 : -width-8) : (app.width-width)/2
                    y:root.vertical ? (app.height-height)/2 : root.options.edge==="top" ? app.height+8 : -height-8
                }
            }
            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; visible: !root.vertical }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; visible: root.vertical }
        }
    }
    Menu {
        id: brandMenu
        popupType:Popup.Window
        palette.window:root.surface; palette.windowText:root.foreground; palette.text:root.foreground
        enter: Transition { NumberAnimation { property:"opacity"; from:0; to:1; duration:root.duration } }
        MenuItem { text: "Открыть Umbrella"; onTriggered: dockSystem.showSettings() }
        MenuItem { text: "Настройки Dock"; onTriggered: dockSystem.showSettings(true) }
        MenuItem { text: "Выключить Dock"; onTriggered: Qt.callLater(function() { dockSystem.setEnabled(false) }) }
        MenuSeparator {}
        MenuItem { text: "Завершить Umbrella"; onTriggered: dockSystem.quit() }
    }
    Menu {
        id: appMenu
        popupType:Popup.Window
        palette.window:root.surface; palette.windowText:root.foreground; palette.text:root.foreground
        enter: Transition { NumberAnimation { property:"opacity"; from:0; to:1; duration:root.duration } }
        MenuItem { text: "Открыть новый экземпляр"; onTriggered: dockSystem.launch(dockSystem.indexOf(root.menuIdentity)) }
        MenuItem { text: root.menuPinned ? "Открепить" : "Закрепить"; onTriggered: dockSystem.pin(dockSystem.indexOf(root.menuIdentity)) }
        MenuItem { text: "Переместить к началу"; enabled: root.menuPinned && root.menuRow>0; onTriggered: {const i=dockSystem.indexOf(root.menuIdentity); dockSystem.movePin(i,i-1)} }
        MenuItem { text: "Переместить к концу"; enabled: root.menuPinned; onTriggered: {const i=dockSystem.indexOf(root.menuIdentity); dockSystem.movePin(i,i+1)} }
        MenuSeparator {}
        Instantiator {
            model: root.menuWindows
            delegate: Menu {
                required property var modelData
                title: (modelData.active ? "● " : "") + modelData.title + " · " + modelData.screen
                MenuItem { text: "Переключиться"; onTriggered: dockSystem.activateWindow(modelData.handle) }
                MenuItem { text: "Закрыть окно"; onTriggered: dockSystem.closeWindow(modelData.handle) }
            }
            onObjectAdded: function(index,object) { appMenu.insertMenu(index+5,object) }
            onObjectRemoved: function(index,object) { appMenu.removeMenu(object) }
        }
    }
}
