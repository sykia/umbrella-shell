import QtQuick
ShaderEffect {
    id: effect
    property var source
    property Item maskItem
    property Item materialOwner
    property var placement: ({})
    property string materialStyle: "mica"
    property real surfaceMode: 0
    property real nativeBackdrop: 0
    SettingsTheme { id: theme }
    property color accent: theme.accent
    property real mica: materialStyle === "mica" ? 1 : 0
    property real dark: theme.dark ? 1 : 0
    property variant wallpaperSource: backdrop
    property real backdropReady: backdrop.status === Image.Ready ? 1 : 0
    property vector2d blurStep: Qt.vector2d(28 / Math.max(100, placement.screenWidth || 1920), 28 / Math.max(100, placement.screenHeight || 1080))
    function sampleRect() {
        const owner = materialOwner || maskItem
        if (!maskItem || !owner) return Qt.vector4d(0,0,1,1)
        // Register all ancestor geometry dependencies, including dragging and
        // Column layout/scale; mapToItem alone does not notify QML bindings.
        let node = maskItem
        let geometry = owner.x + owner.y + owner.width + owner.height
        for (let i = 0; node && i < 32; ++i) {
            geometry += node.x + node.y + node.width + node.height + node.scale
            if (node === owner) break
            node = node.parent
        }
        const a = maskItem.mapToItem(owner, 0, 0)
        const b = maskItem.mapToItem(owner, maskItem.width, maskItem.height)
        const w = Math.max(100, placement.screenWidth || 1920)
        const h = Math.max(100, placement.screenHeight || 1080)
        return Qt.vector4d(((placement.x || 0) + owner.x + a.x) / w,
                          ((placement.y || 0) + owner.y + a.y) / h,
                          (b.x-a.x) / w, (b.y-a.y) / h)
    }
    property vector4d backgroundRect: sampleRect()
    Image {
        id: backdrop
        visible: false
        source: effect.nativeBackdrop > 0 ? "" : "image://widgetBackdrop/" + (effect.placement.screenX || 0) + "," + (effect.placement.screenY || 0) + "," + (effect.placement.screenWidth || 1920) + "," + (effect.placement.screenHeight || 1080) + "/" + (theme.windows ? theme.windows.backdropRevision || 0 : 0)
        asynchronous: true; retainWhileLoading: true
    }
    fragmentShader: "qrc:/materials/glyph.frag.qsb"
}
