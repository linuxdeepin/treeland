// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import Treeland.Protocols

// The popover's surface: the frame, the shell painted under the content, and
// the host every content layer draws into.
Rectangle {
    id: surface

    // The DockPreview controller: geometry, fades and chrome switches.
    required property QtObject preview

    // Scale of the whole popover around `scaleOrigin`. The entry animation
    // decides `shellScale`; the dock edge the frame is glued to decides where
    // it grows from.
    property real shellScale: 1
    property bool shellScaleAnimated: true
    // The frame scales towards its dock-facing edge.
    readonly property point scaleOrigin: preview.scaleOriginFor(width, height)

    // Where the preview view and the tooltip layers draw.
    readonly property alias contentHost: contentHost
    // The pointer is over the popover.
    readonly property alias titleHovered: titleHover.hovered

    z: preview.z
    // Only the frame geometry.
    color: "transparent"
    width: preview.frameWidth
    height: preview.frameHeight
    radius: PopoverStyle.radius
    clip: false
    visible: preview.visible

    transform: Scale {
        xScale: surface.shellScale
        yScale: xScale
        origin.x: surface.scaleOrigin.x
        origin.y: surface.scaleOrigin.y

        Behavior on xScale {
            enabled: surface.shellScaleAnimated && surface.preview.enableAnimation
            NumberAnimation {
                id: scaleAnimation
                duration: surface.preview.aniDuration
                easing.type: Easing.OutExpo
            }
        }
    }

    // Host of every content layer. Each layer owns its opacity, so nothing
    // here multiplies into the shell's fade. The shell paints below them.
    Item {
        id: contentHost
        anchors.fill: parent
        z: 0
    }

    // The group fades on its own value.
    Rectangle {
        id: shell
        anchors.fill: parent
        z: -1
        opacity: surface.preview.shellFade
        clip: false
        color: PopoverStyle.tintFor(surface.preview.enableBlur && backgroundBlur.blurEnabled)
        radius: surface.radius

        XdgShadow {
            visible: surface.preview.enableShadow
            cornerRadius: surface.radius
            shadowBlur: PopoverStyle.shadowBlur
            shadowOffsetX: 0
            shadowOffsetY: PopoverStyle.shadowOffsetY
            shadowColor: PopoverStyle.shadowColor
        }

        Blur {
            id: backgroundBlur
            anchors.fill: parent
            visible: surface.preview.enableBlur
            radius: surface.radius
        }

        Border {
            anchors.fill: parent
            visible: surface.preview.enableBorder
            radius: surface.radius
            outsideColor: PopoverStyle.borderOutsideColor
            insideColor: PopoverStyle.borderInsideColor
        }
    }

    // Covers the whole frame, not only its content: the title row needs the
    // pointer too.
    HoverHandler {
        id: titleHover
        enabled: surface.preview.desiredSurfaces.length !== 0
        onHoveredChanged: {
            if (!hovered) {
                surface.preview.desktop.windowManagement.leaveDockPreview(surface.preview.target.shellSurface.surface)
            } else {
                surface.preview.desktop.windowManagement.enterDockPreview(surface.preview.target.shellSurface.surface)
            }
        }
        cursorShape: Qt.ArrowCursor
    }
}
