import QtQuick
Text {
    id: label
    property string materialStyle: "transparent"
    property bool nativeAvailable: false
    property Item materialOwner: null
    property var materialPlacement: ({})
    SettingsTheme { id: theme }
    WidgetMaterial { id: material; style: label.materialStyle }
    property bool shaderFailed: false
    readonly property bool effectActive: !shaderFailed && material.glass && GraphicsInfo.api !== GraphicsInfo.Unknown && GraphicsInfo.api !== GraphicsInfo.Software
    layer.enabled: effectActive
    layer.smooth: true
    layer.effect: MaterialEffect {
        nativeBackdrop: label.nativeAvailable ? 1 : 0
        maskItem: label; materialOwner: label.materialOwner
        placement: label.materialPlacement; materialStyle: label.materialStyle
        onStatusChanged: if (status === ShaderEffect.Error) label.shaderFailed = true
    }
}
