import QtQuick
Rectangle {
    id: surface
    property string materialStyle: "solid"
    property bool nativeAvailable: false
    property Item materialOwner: null
    property var materialPlacement: ({})
    SettingsTheme { id: theme }
    WidgetMaterial { id: material; style: surface.materialStyle; nativeAvailable: surface.nativeAvailable }
    color: material.surface; radius: 16
    property bool shaderFailed: false
    readonly property bool effectActive: !shaderFailed && material.glass && !nativeAvailable && GraphicsInfo.api !== GraphicsInfo.Unknown && GraphicsInfo.api !== GraphicsInfo.Software
    layer.enabled: effectActive
    layer.effect: MaterialEffect {
        maskItem: surface; materialOwner: surface.materialOwner
        placement: surface.materialPlacement; materialStyle: surface.materialStyle
        surfaceMode: 1
        onStatusChanged: if (status === ShaderEffect.Error) surface.shaderFailed = true
    }
    Behavior on color { ColorAnimation { duration: theme.normal } }
}
