import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    clip: true; contentWidth: availableWidth
    property string selectedScreen: dockSystem.config.screen || (dockSystem.screens.length ? dockSystem.screens[0].name : "")
    property var placement: dockSystem.screenConfig(selectedScreen)
    Connections { target: dockSystem; function onConfigChanged() { page.placement=dockSystem.screenConfig(page.selectedScreen) } }
    onSelectedScreenChanged: placement=dockSystem.screenConfig(selectedScreen)
    component Label: Text { color:"white"; font.family:"Segoe UI"; font.pixelSize:13; wrapMode:Text.WordWrap; Layout.fillWidth:true }
    component Check: CheckBox { palette.windowText:"white"; Layout.fillWidth:true }
    component Range: ColumnLayout {
        property string caption
        property string key
        property int low
        property int high
        Label { text: parent.caption + " · " + Math.round(slider.value) }
        Slider { id:slider; Layout.fillWidth:true; from:parent.low; to:parent.high; stepSize:1; value:dockSystem.config[parent.key]; onMoved:dockSystem.configure(parent.key,Math.round(value)) }
    }
    ColumnLayout {
        width:page.availableWidth; spacing:12
        Label { text:"Umbrella Dock"; font.pixelSize:24; font.weight:Font.DemiBold }
        Check { text:"Включить Dock"; checked:dockSystem.enabled; onClicked:dockSystem.setEnabled(checked) }
        Label { text:"Совместимый режим: системная панель сохраняет Пуск, приложения, трей и часы. Dock не скрывает и не переставляет элементы Explorer."; opacity:0.7 }
        Rectangle {
            Layout.fillWidth:true; implicitHeight:100; radius:14; color:"#34434c"
            Rectangle {
                anchors.centerIn:parent; width:Math.min(parent.width-20,300); height:64; radius:dockSystem.config.radius
                color:dockSystem.theme.background; opacity:dockSystem.config.opacity/100
                Row {
                    anchors.centerIn:parent; spacing:dockSystem.config.spacing
                    Image { source:"qrc:/assets/umbrella.png"; width:40; height:40; fillMode:Image.PreserveAspectFit }
                    Repeater {
                        model:dockSystem
                        Image { required property int index; required property string iconSource; source:iconSource; width:index<4 ? 40 : 0; height:40; visible:index<4; fillMode:Image.PreserveAspectFit }
                    }
                }
            }
            Text { anchors.bottom:parent.bottom; anchors.horizontalCenter:parent.horizontalCenter; text:"Предпросмотр поверхности"; color:"white"; font.pixelSize:10 }
        }
        Label { text:"Расположение"; font.pixelSize:18 }
        ComboBox { Layout.fillWidth:true; model:dockSystem.screens; textRole:"label"; valueRole:"name"; onActivated: { page.selectedScreen=currentValue; dockSystem.configure("screen",currentValue) } }
        Check { text:"Dock на всех подключённых экранах"; checked:dockSystem.config.allScreens; onClicked:dockSystem.configure("allScreens",checked) }
        ComboBox { Layout.fillWidth:true; model:["Снизу","Сверху","Слева","Справа"]; currentIndex:["bottom","top","left","right"].indexOf(page.placement.edge); onActivated:dockSystem.configureScreen(page.selectedScreen,"edge",["bottom","top","left","right"][currentIndex]) }
        ComboBox { Layout.fillWidth:true; model:["К началу","По центру","К концу"]; currentIndex:page.placement.alignment; onActivated:dockSystem.configureScreen(page.selectedScreen,"alignment",currentIndex) }
        Label { text:"Отступ от края · " + margin.value }
        Slider { id:margin; Layout.fillWidth:true; from:0; to:80; stepSize:1; value:page.placement.margin; onMoved:dockSystem.configureScreen(page.selectedScreen,"margin",Math.round(value)) }
        Label { text:"Внешний вид"; font.pixelSize:18 }
        ComboBox { Layout.fillWidth:true; model:["Системная тема","Светлая","Тёмная"]; currentIndex:["auto","light","dark"].indexOf(dockSystem.config.theme); onActivated:dockSystem.configure("theme",["auto","light","dark"][currentIndex]) }
        Range { Layout.fillWidth:true; caption:"Размер значков"; key:"iconSize"; low:24; high:80 }
        Range { Layout.fillWidth:true; caption:"Интервал"; key:"spacing"; low:0; high:24 }
        Range { Layout.fillWidth:true; caption:"Непрозрачность, %"; key:"opacity"; low:20; high:100 }
        Range { Layout.fillWidth:true; caption:"Скругление"; key:"radius"; low:0; high:40 }
        Range { Layout.fillWidth:true; caption:"Увеличение при наведении, %"; key:"magnification"; low:0; high:40 }
        Check { text:"Системный backdrop при доступности"; checked:dockSystem.config.blur; onClicked:dockSystem.configure("blur",checked) }
        Check { text:"Индикаторы работающих приложений"; checked:dockSystem.config.indicators; onClicked:dockSystem.configure("indicators",checked) }
        Label { text:"Поведение"; font.pixelSize:18 }
        ComboBox { Layout.fillWidth:true; model:["Всегда показывать","Автоскрытие","Скрывать при перекрытии активным окном"]; currentIndex:dockSystem.config.hideMode; onActivated:dockSystem.configure("hideMode",currentIndex) }
        Range { Layout.fillWidth:true; caption:"Задержка скрытия, мс"; key:"hideDelay"; low:100; high:3000 }
        Range { Layout.fillWidth:true; caption:"Задержка появления, мс"; key:"revealDelay"; low:100; high:1000 }
        Check { text:"Скрывать для полноэкранных приложений"; checked:dockSystem.config.hideFullscreen; onClicked:dockSystem.configure("hideFullscreen",checked) }
        Check { text:"Показывать незакреплённые запущенные приложения"; checked:dockSystem.config.showRunning; onClicked:dockSystem.configure("showRunning",checked) }
        Check { text:"Анимации (с учётом настроек Windows)"; checked:dockSystem.config.animations; onClicked:dockSystem.configure("animations",checked) }
        Range { Layout.fillWidth:true; caption:"Скорость анимаций, %"; key:"speed"; low:50; high:200 }
        Label { text:"Приложения"; font.pixelSize:18 }
        Label { text:"ЛКМ — запуск или переключение; повторный клик — сворачивание одного окна или следующее окно группы. Средняя кнопка — новый экземпляр. ПКМ — окна и закрепления. Закреплённые значки можно перетаскивать."; opacity:0.7 }
        TextField { id:appPath; Layout.fillWidth:true; placeholderText:"Полный путь к .exe, .lnk или .appref-ms"; onAccepted:if(dockSystem.addApplication(text)) text="" }
        Button { text:"Добавить приложение"; onClicked:if(dockSystem.addApplication(appPath.text)) appPath.text="" }
        Button { text:"Импорт доступных закреплений Windows"; onClicked:dockSystem.importWindowsPins() }
        Label { text:dockSystem.status; opacity:0.8 }
    }
}
