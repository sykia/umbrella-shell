import QtQuick
import QtQuick.Window
import QtQuick.Controls

Rectangle {
    id: root
    SettingsTheme { id: theme }
    property bool backdropAvailable: false
    property bool firstFrameReady: false
    property int phase: 0 // loading, settling, entrance, home
    property bool homeOpen: true
    property bool taskbarExpanded: false
    property string taskbarSection: "settings"
    property real brandSweep: 0
    property real homePosition: 0
    readonly property real sidebarWidth: Math.min(220, root.width * 0.28)
    readonly property real contentLeft: 24 + sidebarWidth
    readonly property real contentCenter: contentLeft + (root.width - contentLeft - 12) / 2
    property real reveal: 0
    readonly property bool interfaceReady: firstFrameReady
        && settingsLoader.status === Loader.Ready && logo.status === Image.Ready
    width: 1000
    height: 700
    color: root.backdropAvailable && theme.transparency ? Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.35) : theme.background
    radius: 8
    border.width: 1
    border.color: theme.line
    focus: true
    Keys.onEscapePressed: root.Window.window.close()

    onInterfaceReadyChanged: if (interfaceReady && !theme.animations) finishLoading()
    Connections {
        target: theme
        function onAnimationsChanged() {
            if (!theme.animations) {
                startupSpin.stop(); waitingSpin.stop(); settle.stop()
                revealAnimation.stop(); brandEntrance.stop()
                if (root.interfaceReady) {
                    root.reveal = 1; root.brandSweep = 1; root.homePosition = 1; root.phase = 3
                }
            }
        }
    }
    function finishLoading() {
        if (!interfaceReady || phase !== 0)
            return
        if (!theme.animations) {
            reveal = 1; brandSweep = 1; homePosition = 1; phase = 3
            return
        }
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
                radius: 8
                color: root.backdropAvailable && theme.transparency ? "transparent" : theme.soft
            }
            Rectangle {
                x: 24 + root.sidebarWidth; y: 12
                width: parent.width - x - 12
                height: parent.height - 24
                radius: 8
                color: "transparent"
            }
        }
    }

    Rectangle {
        id: taskbarNav
        objectName: "taskbarNavigation"
        activeFocusOnTab: true
        Keys.onSpacePressed: navMouse.clicked(null)
        Keys.onReturnPressed: navMouse.clicked(null)
        border.width: activeFocus ? 2 : 0; border.color: theme.accent
        x: 24; y: 96
        width: root.sidebarWidth - 24; height: 40; radius: 5
        visible: root.phase === 3
        color: !root.homeOpen && root.taskbarSection !== "widgets" ? theme.soft : navMouse.containsMouse ? theme.hover : "transparent"
        scale: navMouse.pressed ? 0.98 : navMouse.containsMouse ? 1 : 1
        Behavior on color { ColorAnimation { duration: theme.animations ? 180 : 0 } }
        Behavior on scale { NumberAnimation { duration: theme.animations ? 150 : 0; easing.type: Easing.OutCubic } }
        Rectangle { x: 0; anchors.verticalCenter: parent.verticalCenter; width: 3; height: 18; radius: 2; color: theme.accent; visible: !root.homeOpen && root.taskbarSection !== "widgets" }
        Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: "\uE7F4"; font.family: "Segoe Fluent Icons"; font.pixelSize: 16; color: theme.ink }
        Text {
            anchors.centerIn: parent; text: "Панель задач"
            color: !root.homeOpen && root.taskbarSection !== "widgets" ? theme.ink : theme.ink; font.family: "Segoe UI Variable"; font.pixelSize: 14
        }
        Text {
            anchors.right:parent.right; anchors.rightMargin:18; anchors.verticalCenter:parent.verticalCenter
            text:"\uE70D"; color:theme.ink; font.family:"Segoe Fluent Icons"; font.pixelSize:10
            rotation:root.taskbarExpanded ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: theme.animations ? 180 : 0; easing.type:Easing.OutCubic } }
        }
        MouseArea {
            id: navMouse
            anchors.fill: parent; hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: { root.taskbarExpanded = !root.taskbarExpanded; if(root.taskbarSection === "widgets") root.taskbarSection="settings"; root.homeOpen = false }
            Accessible.role: Accessible.Button
            Accessible.name: "Taskbar"
            Accessible.onPressAction: { root.taskbarExpanded = !root.taskbarExpanded; if(root.taskbarSection === "widgets") root.taskbarSection="settings"; root.homeOpen = false }
        }
    }
    Column {
        x:30; y:144; width:root.sidebarWidth - 36; spacing:5
        height:root.taskbarExpanded ? 81 : 0
        opacity:root.taskbarExpanded ? 1 : 0
        visible:root.phase === 3 && height > 0; clip:true
        Behavior on height { NumberAnimation { duration: theme.animations ? 180 : 0; easing.type:Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: theme.animations ? 160 : 0 } }
        Repeater {
            model:[{key:"settings",label:"Основные настройки"},{key:"configuration",label:"Конфигурация панели"}]
            Rectangle {
                required property var modelData
                objectName:"taskbarNavigation_" + modelData.key
                activeFocusOnTab: true
                Keys.onSpacePressed: subMouse.clicked(null)
                Keys.onReturnPressed: subMouse.clicked(null)
                border.width: activeFocus ? 2 : 0; border.color: theme.accent
                width:parent.width; height:38; radius:5
                color:!root.homeOpen && root.taskbarSection === modelData.key ? theme.tint : subMouse.containsMouse ? theme.hover : "transparent"
                Behavior on color { ColorAnimation { duration: theme.animations ? 150 : 0 } }
                Text { anchors.fill:parent; anchors.leftMargin:12; verticalAlignment:Text.AlignVCenter; text:modelData.label; color:theme.ink; font.family:"Segoe UI Variable"; font.pixelSize:12 }
                MouseArea {
                    id:subMouse; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor
                    onClicked: { root.homeOpen=false; root.taskbarSection=modelData.key }
                    Accessible.role:Accessible.Button; Accessible.name:modelData.label
                    Accessible.onPressAction: { root.homeOpen=false; root.taskbarSection=modelData.key }
                }
            }
        }
    }
    FluentButton {
        objectName: "widgetsNavigation"
        x: 24; y: root.taskbarExpanded ? 236 : 144
        width: root.sidebarWidth - 24; height: 40
        visible: root.phase === 3
        text: "Widgets"
        onClicked: { root.homeOpen=false; root.taskbarSection="widgets"; root.taskbarExpanded=false }
    }
    Loader {
        x: root.contentLeft + 14; y: 28; width: root.width-x-26; height: root.height-54
        active: root.phase === 3 && !root.homeOpen && root.taskbarSection === "widgets"
        sourceComponent: WidgetsPage {}
    }
    WidgetHost {}
    ScrollView {
        id: configurationScroll
        objectName: "configurationPageScroll"
        x:root.contentLeft + 14; y:28; width:root.width-x-26; height:root.height-54
        opacity:root.phase === 3 && !root.homeOpen && root.taskbarSection === "configuration" ? 1 : 0
        visible:opacity > 0; enabled:!root.homeOpen && root.taskbarSection === "configuration"
        clip:true; contentWidth:availableWidth
        ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
        Behavior on opacity { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }
        transform: Translate { y: (1-configurationScroll.opacity)*12 }
        TaskbarConfiguration { width:parent.width; system:taskbarSystem; scrollTarget:configurationScroll.contentItem }
    }
    TaskbarPage {
        id: taskbarPage
        system: taskbarSystem
        x: root.contentLeft + 14; y: 28
        width: root.width - x - 26; height: root.height - 54
        opacity: root.phase === 3 && !root.homeOpen && root.taskbarSection === "settings" ? 1 : 0
        visible: opacity > 0
        enabled: !root.homeOpen && root.phase === 3 && root.taskbarSection === "settings"
        transform: Translate { y: (1-taskbarPage.opacity)*12 }
        Behavior on opacity { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }
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
        Behavior on scale { NumberAnimation { duration: theme.animations ? 180 : 0; easing.type: Easing.OutCubic } }
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
                text: "Umbrella"
                font.family: "Segoe UI Variable"
                font.pixelSize: Math.min(25, (brand.width - 24) / 4.2)
                font.weight: Font.DemiBold
                color: theme.ink
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

    ScrollView {
        id: homePage
        x: root.contentLeft + 28; y: 54
        width: root.width - x - 40; height: root.height-y-24
        clip: true; contentWidth: availableWidth
        opacity: root.phase === 3 && root.homeOpen ? 1 : 0
        visible: opacity > 0
        enabled: root.phase === 3 && root.homeOpen
        transform: Translate { y: (1-homePage.opacity)*12 }
        Behavior on opacity { NumberAnimation { duration: theme.normal; easing.type: Easing.OutCubic } }
        Column { width: parent.width; spacing: 20
        Text { text: "ПЕРСОНАЛИЗАЦИЯ"; color: theme.muted; font.family: "Segoe UI Variable"; font.pixelSize: 11; font.letterSpacing: 1.5 }
        Text { text: "Настройки Umbrella"; color: theme.ink; font.family: "Segoe UI Variable"; font.pixelSize: 30; font.weight: Font.DemiBold; width: parent.width; wrapMode: Text.WordWrap }
        Text { text: "Сделайте привычный Windows удобнее для себя."; color: theme.muted; font.pixelSize: 14; width: parent.width; wrapMode: Text.WordWrap }
        Repeater {
            model: [{title:"Панель задач", detail:"Вид, расположение, размеры и прозрачность", key:"settings", glyph:"\uE7F4"},
                    {title:"Конфигурация панели", detail:"Расположите компоненты на каждом экране", key:"configuration", glyph:"\uE8A9"},
                    {title:"Widgets", detail:"Каталог и настройка рабочего пространства", key:"widgets", glyph:"\uE8A9"}]
            FluentButton {
                id: homeCard
                required property var modelData
                width: parent.width; height: 94; hoverEnabled: true
                onClicked: { root.homeOpen=false; root.taskbarSection=modelData.key; root.taskbarExpanded=modelData.key!=="widgets" }
                background: Rectangle { radius: 8; color: homeCard.hovered ? theme.hover : theme.card
                    border.width: homeCard.visualFocus ? 2 : 1; border.color: homeCard.visualFocus ? theme.accent : theme.line
                    Behavior on color { ColorAnimation { duration: theme.fast } } }
                contentItem: Item {
                    Rectangle { x: 16; y: 17; width: 48; height: 48; radius: 8; color: theme.tint
                        Text { anchors.centerIn: parent; text: homeCard.modelData.glyph; font.family: "Segoe Fluent Icons"; font.pixelSize: 21; color: theme.ink }
                    }
                    Column { x: 80; y: 18; width: parent.width-112; spacing: 7
                        Text { text: homeCard.modelData.title; color: theme.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Text { text: homeCard.modelData.detail; color: theme.muted; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
                    }
                }
                Accessible.name: modelData.title
            }
        }
        Text { text: "●  Цвет и оформление следуют теме Windows"; color: theme.muted; font.pixelSize: 12; width: parent.width; wrapMode: Text.WordWrap }
    }
    }
    FluentButton {
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16
        width: 34; height: 30; text: "×"; font.pixelSize: 22
        onClicked: root.Window.window.close()
        Accessible.name: "Скрыть настройки"
        palette.buttonText: theme.ink
        background: Rectangle { radius: 5; color: parent.hovered ? "#c42b1c" : "transparent" }
    }

    Image {
        id: logo
        objectName: "homeLogo"
        readonly property real entranceX: 28 + (root.sidebarWidth - 44) * root.brandSweep
        width: 124 - 76 * root.reveal + 76 * root.homePosition
        height: width
        x: (root.width - width) / 2 * (1 - root.reveal)
           + ((1 - root.homePosition) * entranceX
              + root.homePosition * (root.contentCenter - width / 2)) * root.reveal
        y: (root.height - height) / 2 * (1 - root.reveal)
           + ((1 - root.homePosition) * 32
              + root.homePosition * (root.height - height) / 2) * root.reveal
        visible: root.phase !== 3
        source: "qrc:/assets/umbrella.png"
        sourceSize: Qt.size(192, 192)
        fillMode: Image.PreserveAspectFit
        mipmap: true
        scale: logoHover.hovered && root.phase === 3 ? 1.06 : 1
        Behavior on scale { NumberAnimation { duration: theme.animations ? 200 : 0; easing.type: Easing.OutCubic } }
        HoverHandler { id: logoHover }
    }

    NumberAnimation {
        target: logo; property: "rotation"
        from: 0; to: 360; duration: theme.animations ? 14000 : 0
        loops: Animation.Infinite
        running: false && root.phase === 3 && root.homeOpen && root.visible
                 && root.Window.window !== null && root.Window.window.visible
    }

    SequentialAnimation {
        id: startupSpin
        running: theme.animations && root.phase === 0 && root.visible
                 && root.Window.window !== null && root.Window.window.visible
        NumberAnimation {
            target: logo; property: "rotation"
            from: 0; to: 180; duration: theme.animations ? 120 : 0
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
            from: 180; to: 540; duration: theme.animations ? 1400 : 0
        }
        ScriptAction { script: root.finishLoading() }
    }
    NumberAnimation {
        id: settle
        target: logo; property: "rotation"
        duration: theme.animations ? 120 : 0
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
        from: 0; to: 1; duration: theme.animations ? 200 : 0
        easing.type: Easing.InOutCubic
        onFinished: brandEntrance.start()
    }
    SequentialAnimation {
        id: brandEntrance
        NumberAnimation {
            target: root; property: "brandSweep"
            from: 0; to: 1; duration: theme.animations ? 120 : 0
            easing.type: Easing.InOutCubic
        }
        NumberAnimation {
            target: root; property: "homePosition"
            from: 0; to: 1; duration: theme.animations ? 120 : 0
            easing.type: Easing.InOutCubic
        }
        ScriptAction { script: root.phase = 3 }
    }
}
