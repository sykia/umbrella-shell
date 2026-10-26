import QtQuick
QtObject {
    property string style: "solid"
    property bool nativeAvailable: false
    readonly property SettingsTheme theme: SettingsTheme {}
    readonly property bool glass: theme.transparency && (style === "acrylic" || style === "mica")
    readonly property color surface: style === "transparent" ? "transparent" : glass && nativeAvailable ? Qt.rgba(theme.card.r, theme.card.g, theme.card.b, style === "mica" ? 0.02 : 0.14) : theme.card
    readonly property color edge: style === "transparent" ? "transparent" : theme.line
    readonly property color highlight: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, style === "mica" ? 0.32 : 0.15)
}
