import QtQuick

QtObject {
    id: theme
    objectName: "settingsTheme"
    readonly property SystemPalette systemPalette: SystemPalette {}
    readonly property var windows: typeof systemTheme !== "undefined" ? systemTheme.theme : null
    readonly property bool dark: windows ? windows.dark : systemPalette.window.hslLightness < 0.5
    readonly property bool contrast: windows ? windows.contrast : false
    readonly property color accent: windows ? windows.accent : systemPalette.accent
    readonly property bool animations: !contrast && (!windows || windows.animations !== false)
    readonly property bool transparency: !contrast && !!windows && windows.transparency === true
    readonly property int fast: animations ? 120 : 0
    readonly property int normal: animations ? 200 : 0
    readonly property color ink: contrast ? windows.foreground : dark ? "#ffffff" : "#1a1a1a"
    readonly property color muted: contrast ? windows.foreground : dark ? "#c8c8c8" : "#5d5d5d"
    readonly property color background: contrast ? windows.background : dark ? "#202020" : "#f3f3f3"
    readonly property color card: contrast ? windows.background : dark ? "#2b2b2b" : "#ffffff"
    readonly property color soft: contrast ? windows.background : dark ? "#323232" : "#ededed"
    readonly property color hover: contrast ? windows.background : dark ? "#383838" : "#e5e5e5"
    readonly property color line: contrast ? windows.foreground : dark ? "#454545" : "#dedede"
    readonly property color tint: Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.23 : 0.12)
    readonly property color onAccent: accent.r * 0.299 + accent.g * 0.587 + accent.b * 0.114 > 0.58 ? "#101820" : "#ffffff"
}
