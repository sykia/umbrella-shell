import QtQuick
Rectangle {
    id: bar
    readonly property bool vertical: shell.panelEdge === "left" || shell.panelEdge === "right"
    color: "#f0f1f6"
    PanelGroup { zone: 0; logo: true; vertical: bar.vertical; x: 5; y: 5 }
    PanelGroup {
        zone: 1; vertical: bar.vertical
        x: bar.vertical ? 5 : (bar.width - width) / 2
        y: bar.vertical ? (bar.height - height) / 2 : 5
    }
    PanelGroup {
        zone: 2; vertical: bar.vertical
        x: bar.vertical ? 5 : bar.width - width - 5
        y: bar.vertical ? bar.height - height - 5 : 5
    }
}
