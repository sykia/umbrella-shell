import QtQuick
import QtQuick.Window
import QtQuick.Controls

Rectangle {
    id: root
    property bool backdropAvailable: false
    property bool firstFrameReady: false
    property int phase: 0 // loading, settling, entrance, home
    property bool homeOpen: true
    property bool taskbarExpanded: false
    property string taskbarSection: "settings"
    property real brandSweep: 0
    property real homeDock: 0
    readonly property real sidebarWidth: Math.min(220, root.width * 0.28)
    readonly property real contentLeft: 24 + sidebarWidth
    readonly property real contentCenter: contentLeft + (root.width - contentLeft - 12) / 2
    property real reveal: 0
    readonly property bool interfaceReady: firstFrameReady
        && settingsLoader.status === Loader.Ready && logo.status === Image.Ready
    width: 1000
    height: 700
    color: backdropAvailable ? "#d9242630" : "#242630"
    focus: true
    Keys.onEscapePressed: root.Window.window.close()

    function finishLoading() {
        if (!interfaceReady || phase !== 0)
            return
        phase = 1
        settle.from = logo.rotation
        settle.to = Math.ceil((logo.rotation + 1) / 360) * 360
        settle.start()
    }

    DragHandler {
        target: null
        onActiveChanged: if (active) root.Window.window.startSystemMove()
    }

    Loader {
        id: settingsLoader
        anchors.fill: parent
        asynchronous: true
        opacity: root.reveal
        visible: opacity > 0
        sourceComponent: Item {
            // Navigation and content remain empty apart from the home branding.
            Rectangle {
                x: 12; y: 12
                width: root.sidebarWidth
                height: parent.height - 24
                radius: 16
                color: "#16ffffff"
            }
            Rectangle {
                x: 24 + root.sidebarWidth; y: 12
                width: parent.width - x - 12
                height: parent.height - 24
                radius: 16
                color: "#08ffffff"
            }
        }
    }

    Rectangle {
        id: taskbarNav
        objectName: "taskbarNavigation"
        x: 24; y: 96
        width: root.sidebarWidth - 24; height: 40; radius: 12
        visible: root.phase === 3
        color: !root.homeOpen && root.taskbarSection !== "dock" ? "#e7131b" : navMouse.containsMouse ? "#28ffffff" : "#10ffffff"
        scale: navMouse.pressed ? 0.98 : navMouse.containsMouse ? 1.02 : 1
        Behavior on color { ColorAnimation { duration: 180 } }
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Text {
            anchors.centerIn: parent; text: "Taskbar"
            color: "white"; font.family: "Segoe UI"; font.pixelSize: 14
        }
        Text {
            anchors.right:parent.right; anchors.rightMargin:18; anchors.verticalCenter:parent.verticalCenter
            text:"\uE70D"; color:"white"; font.family:"Segoe MDL2 Assets"; font.pixelSize:10
            rotation:root.taskbarExpanded ? 180 : 0
            Behavior on rotation { NumberAnimation { duration:180; easing.type:Easing.OutCubic } }
        }
        MouseArea {
            id: navMouse
            anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: { root.taskbarExpanded = !root.taskbarExpanded; if(root.taskbarSection === "dock") root.taskbarSection="settings"; root.homeOpen = false }
            Accessible.role: Accessible.Button
            Accessible.name: "Taskbar"
            Accessible.onPressAction: { root.taskbarExpanded = !root.taskbarExpanded; if(root.taskbarSection === "dock") root.taskbarSection="settings"; root.homeOpen = false }
        }
    }
    Column {
        x:30; y:144; width:root.sidebarWidth - 36; spacing:5
        height:root.taskbarExpanded ? 81 : 0
        opacity:root.taskbarExpanded ? 1 : 0
        visible:root.phase === 3 && height > 0; clip:true
        Behavior on height { NumberAnimation { duration:180; easing.type:Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration:160 } }
        Repeater {
            model:[{key:"settings",label:"Основные настройки"},{key:"configuration",label:"Конфигурация панели"}]
            Rectangle {
                required property var modelData
                objectName:"taskbarNavigation_" + modelData.key
                width:parent.width; height:38; radius:10
                color:!root.homeOpen && root.taskbarSection === modelData.key ? "#45e7131b" : subMouse.containsMouse ? "#28ffffff" : "transparent"
                Behavior on color { ColorAnimation { duration:150 } }
                Text { anchors.fill:parent; anchors.leftMargin:12; verticalAlignment:Text.AlignVCenter; text:modelData.label; color:"white"; font.family:"Segoe UI"; font.pixelSize:12 }
                MouseArea {
                    id:subMouse; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor
                    onClicked: { root.homeOpen=false; root.taskbarSection=modelData.key }
                    Accessible.role:Accessible.Button; Accessible.name:modelData.label
                    Accessible.onPressAction: { root.homeOpen=false; root.taskbarSection=modelData.key }
                }
            }
        }
    }
    Rectangle {
        objectName: "dockNavigation"
        x:24; y:root.taskbarExpanded ? 236 : 144
        width:root.sidebarWidth-24; height:40; radius:12
        visible:root.phase === 3
        color:!root.homeOpen && root.taskbarSection === "dock" ? "#e7131b" : dockMouse.containsMouse ? "#28ffffff" : "#10ffffff"
        Behavior on y { NumberAnimation { duration:180; easing.type:Easing.OutCubic } }
        Text { anchors.centerIn:parent; text:"Dock"; color:"white"; font.family:"Segoe UI"; font.pixelSize:14 }
        MouseArea {
            id:dockMouse; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor
            onClicked: { root.homeOpen=false; root.taskbarSection="dock"; root.taskbarExpanded=false }
            Accessible.role:Accessible.Button; Accessible.name:"Dock"
            Accessible.onPressAction: { root.homeOpen=false; root.taskbarSection="dock"; root.taskbarExpanded=false }
        }
    }
    ScrollView {
        id: configurationScroll
        objectName: "configurationPageScroll"
        x:root.contentLeft + 14; y:28; width:root.width-x-26; height:root.height-54
        opacity:root.phase === 3 && !root.homeOpen && root.taskbarSection === "configuration" ? 1 : 0
        visible:opacity > 0; enabled:!root.homeOpen && root.taskbarSection === "configuration"
        clip:true; contentWidth:availableWidth
        ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        Behavior on opacity { NumberAnimation { duration:200 } }
        TaskbarConfiguration { width:parent.width; system:taskbarSystem; scrollTarget:configurationScroll.contentItem }
    }
    Loader {
        x: root.contentLeft + 14; y: 28; width: root.width-x-26; height: root.height-54
        active: typeof dockSystem !== "undefined" && root.phase === 3 && !root.homeOpen && root.taskbarSection === "dock"
        sourceComponent: DockPage {}
    }
    TaskbarPage {
        id: taskbarPage
        system: taskbarSystem
        x: root.contentLeft + 14; y: 28
        width: root.width - x - 26; height: root.height - 54
        opacity: root.phase === 3 && !root.homeOpen && root.taskbarSection === "settings" ? 1 : 0
        visible: opacity > 0
        enabled: !root.homeOpen && root.phase === 3 && root.taskbarSection === "settings"
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    }

    Item {
        id: brand
        objectName: "homeBrand"
        x: 12; y: 36
        width: root.sidebarWidth
        height: 40
        visible: root.phase >= 2
        scale: brandMouse.containsMouse ? 1.04 : 1
        transformOrigin: Item.Center
        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Item {
            anchors.left: parent.left
            height: parent.height
            width: parent.width * root.brandSweep
            clip: true
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: brand.width
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                text: 'Umbre<span style="color:#e7131b">ll</span>a'
                font.family: "Segoe UI"
                font.pixelSize: Math.min(25, (brand.width - 24) / 4.2)
                font.weight: Font.DemiBold
                color: "#ffffff"
            }
        }
        MouseArea {
            id: brandMouse
            anchors.fill: parent
            enabled: root.phase === 3
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.homeOpen = true
            Accessible.role: Accessible.Button
            Accessible.name: "Umbrella"
            Accessible.onPressAction: root.homeOpen = true
        }
    }

    Image {
        id: logo
        objectName: "homeLogo"
        readonly property real entranceX: 28 + (root.sidebarWidth - 44) * root.brandSweep
        width: 124 - 76 * root.reveal + 76 * root.homeDock
        height: width
        x: (root.width - width) / 2 * (1 - root.reveal)
           + ((1 - root.homeDock) * entranceX
              + root.homeDock * (root.contentCenter - width / 2)) * root.reveal
        y: (root.height - height) / 2 * (1 - root.reveal)
           + ((1 - root.homeDock) * 32
              + root.homeDock * (root.height - height) / 2) * root.reveal
        visible: root.phase !== 3 || root.homeOpen
        source: "qrc:/assets/umbrella.png"
        sourceSize: Qt.size(192, 192)
        fillMode: Image.PreserveAspectFit
        mipmap: true
        scale: logoHover.hovered && root.phase === 3 ? 1.06 : 1
        Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        HoverHandler { id: logoHover }
    }

    NumberAnimation {
        target: logo; property: "rotation"
        from: 0; to: 360; duration: 14000
        loops: Animation.Infinite
        running: root.phase === 3 && root.homeOpen && root.visible
                 && root.Window.window !== null && root.Window.window.visible
    }

    SequentialAnimation {
        id: startupSpin
        running: root.phase === 0 && root.visible
                 && root.Window.window !== null && root.Window.window.visible
        NumberAnimation {
            target: logo; property: "rotation"
            from: 0; to: 180; duration: 700
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                if (root.interfaceReady) root.finishLoading()
                else waitingSpin.start()
            }
        }
    }
    SequentialAnimation {
        id: waitingSpin
        loops: Animation.Infinite
        NumberAnimation {
            target: logo; property: "rotation"
            from: 180; to: 540; duration: 1400
        }
        ScriptAction { script: root.finishLoading() }
    }
    NumberAnimation {
        id: settle
        target: logo; property: "rotation"
        duration: 700
        easing.type: Easing.OutQuad
        onStarted: waitingSpin.stop()
        onFinished: {
            logo.rotation = 0
            root.phase = 2
            revealAnimation.start()
        }
    }
    NumberAnimation {
        id: revealAnimation
        target: root; property: "reveal"
        from: 0; to: 1; duration: 480
        easing.type: Easing.InOutCubic
        onFinished: brandEntrance.start()
    }
    SequentialAnimation {
        id: brandEntrance
        NumberAnimation {
            target: root; property: "brandSweep"
            from: 0; to: 1; duration: 650
            easing.type: Easing.InOutCubic
        }
        NumberAnimation {
            target: root; property: "homeDock"
            from: 0; to: 1; duration: 750
            easing.type: Easing.InOutCubic
        }
        ScriptAction { script: root.phase = 3 }
    }
}
