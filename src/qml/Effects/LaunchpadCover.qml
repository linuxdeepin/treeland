import QtQuick
import QtQuick.Effects
import Treeland
import Waylib.Server

Item {
    id: root

    required property SurfaceWrapper wrapper
    required property WaylandOutput output

    property bool mapped: false

    z: (wrapper.z ?? 0) - 1
    anchors.fill: wrapper

    WallpaperController {
        id: wallpaperController
        output: root.output
        lock: true
        type: mapped ? WallpaperController.Scale : WallpaperController.Normal
    }

    ShaderEffectSource {
        id: wallpaper
        sourceItem: wallpaperController.proxy
        recursive: true
        live: true
        smooth: true
        anchors.fill: parent
        hideSource: false
    }

    Blur {
        anchors.fill: parent
        z: wallpaper.z + 1
    }

    Rectangle {
        id: cover
        anchors.fill: parent
        color: 'black'
        opacity: 0.6
    }

    opacity: root.mapped ? 1 : 0
    Behavior on opacity {
        PropertyAnimation {
            duration: 400
            easing.type: Easing.OutExpo
        }
    }
}
