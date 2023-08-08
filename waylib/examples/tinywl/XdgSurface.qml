import QtQuick
import Waylib.Server

XdgSurfaceItem {
    required property WaylandXdgSurface waylandSurface
    required property OutputLayout outputLayout

    surface: waylandSurface

    OutputLayoutItem {
        anchors.fill: parent
        layout: outputLayout

        onEnterOutput: function(output) {
            waylandSurface.surface.enterOutput(output);
        }
        onLeaveOutput: function(output) {
            waylandSurface.surface.leaveOutput(output);
        }
    }
}
