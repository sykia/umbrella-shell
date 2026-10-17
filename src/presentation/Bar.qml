import QtQuick
Rectangle {
    id: bar
    readonly property bool vertical: shell.panelEdge === "left" || shell.panelEdge === "right"
    color: "#21212b"
    PanelGroup { zone: 0; logo: true; vertical: bar.vertical; x: 8; y: 8 }
    PanelGroup {
        zone: 1; vertical: bar.vertical
        x: bar.vertical ? 8 : (bar.width - width) / 2
        y: bar.vertical ? (bar.height - height) / 2 : 8
    }
    PanelGroup {
        zone: 2; vertical: bar.vertical
        x: bar.vertical ? 8 : bar.width - width - 8
        y: bar.vertical ? bar.height - height - 8 : 8
    }
}
