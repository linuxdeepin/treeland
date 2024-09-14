import QtQuick
import QtQuick.Effects
import Waylib.Server
import org.deepin.dtk 1.0 as D

Item {
    id: root
    property int radius
    anchors.fill: parent
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "white"
        opacity: 0.1
    }
    RenderBufferBlitter {
        id: blitter
        z: root.parent.z - 1
        anchors.fill: parent
        MultiEffect {
            id: blur
            anchors.fill: parent
            source: blitter.content
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            saturation: 0.2
        }
    }
}
