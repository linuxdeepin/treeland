// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Waylib.Server
import Treeland

Item {
    id: root

    property real radius: 0
    property rect frameRect: Qt.rect(0, 0, width, height)
    property bool radiusEnabled: radius > 0
    property QtObject effectConfig: WaylandServer.config
    readonly property bool backgroundAllowed: effectConfig?.backgroundEffectEnabled !== undefined
        ? WaylandServer.isEffectEnabled(WaylandServer.effectLevel, effectConfig.backgroundEffectEnabled)
        : true
    property int blurMax: effectConfig?.blurStrength ?? 0
    property bool blurEnabled: backgroundAllowed && (effectConfig?.blurStrength ?? 0) > 0
        && blurMax > 0 && blurAmount > 0
    property real blurAmount: effectConfig?.blurAmount ?? 0
    property real multiplier: effectConfig?.blurMultiplier ?? 0
    property real brightness: 0.0
    property real contrast: 0.0
    property real saturation: effectConfig?.glassSaturation ?? 0
    property real glassSpecular: effectConfig?.glassSpecular ?? 0
    property real glassTint: 0.0
    property bool glassEnabled: effectConfig?.glassEnabled ?? false
    property Item effectContent: blitterLoader.item?.effectContent ?? null
    property bool contentOffscreen: false
    property bool effectEnabled: true
    readonly property Item content: blitterLoader.item?.content ?? null
    property alias offscreen: root.contentOffscreen
    default property alias data: contentContainer.data

    z: parent ? (parent.z ? parent.z - 1 : -1) : -1
    anchors.fill: parent

    // Unload RenderBufferBlitter entirely when background effects are disabled
    // so it neither allocates a backing texture nor blits the framebuffer.
    Loader {
        id: blitterLoader
        anchors.fill: parent
        active: root.backgroundAllowed && root.effectEnabled
        sourceComponent: RenderBufferBlitter {
            id: blitter
            anchors.fill: parent
            smooth: true
            offscreen: root.contentOffscreen

            readonly property Item effectContent: contentLoader

            // Dispatch between Liquid Glass and traditional blur via a Loader so only
            // the active branch is instantiated.  Toggling the DConfig key unloads one
            // Component and loads the other.
            Loader {
                id: contentLoader
                anchors.fill: parent
                sourceComponent: root.glassEnabled ? glassComponent : blurComponent
                visible: !root.contentOffscreen
            }

            Component {
                id: glassComponent
                GlassEffect {
                    anchors.fill: parent
                    source: blitter.content
                    radius: root.radiusEnabled ? root.radius : 0
                    frameRect: root.frameRect
                    blurEnabled: root.blurEnabled
                    blurMax: root.blurMax
                    blurAmount: root.blurAmount
                    blurMultiplier: root.multiplier
                    bezelWidth: root.effectConfig?.glassBezel ?? 30
                    thickness: root.effectConfig?.glassThickness ?? 30
                    ior: 1.33
                    specular: root.glassSpecular
                    tint: root.glassTint
                    brightness: root.brightness
                    contrast: root.contrast
                    saturation: root.saturation
                    contentEdgePull: 0.0
                    contentRampEnd: 0.0
                    refractionMaxTan: 3.3
                    profilePower: root.effectConfig?.glassProfilePower ?? 2.0
                    innerShadow: 0.0
                }
            }

            Component {
                id: blurComponent
                Item {
                    anchors.fill: parent

                    MultiEffect {
                        id: blur
                        anchors.fill: parent
                        layer.enabled: root.radiusEnabled
                        smooth: root.radiusEnabled
                        opacity: root.radiusEnabled ? 0 : 1
                        source: blitter.content
                        autoPaddingEnabled: false
                        blurEnabled: root.blurEnabled
                        blur: root.blurAmount
                        blurMax: root.blurMax
                        blurMultiplier: root.multiplier
                        saturation: 0.2
                    }

                    Loader {
                        anchors.fill: parent
                        active: root.radiusEnabled
                        sourceComponent: FrameClip {
                            source: blur
                            geometry: ({ rect: root.frameRect, radius: root.radius })
                        }
                    }
                }
            }
        }
    }

    Item {
        id: contentContainer
        anchors.fill: parent
    }
}
