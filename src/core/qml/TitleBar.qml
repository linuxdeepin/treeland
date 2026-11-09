// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Waylib.Server
import Treeland
import org.deepin.dtk 1.0 as D

Control {
    id: root

    required property SurfaceWrapper surface
    readonly property SurfaceItem surfaceItem: surface.surfaceItem
    readonly property bool canToggleMaximize: surface.surfaceState === SurfaceWrapper.State.Maximized || surface.isMaximizable
    readonly property bool noRadius: surface.radius === 0 || surface.noCornerRadius || GraphicsInfo.api === GraphicsInfo.Software
    property D.Palette backgroundColor: D.Palette {
        normal: ("#ffffff")
        normalDark: ("#282828")
    }
    property D.Palette textColor: D.Palette {
        normal: ("#303030")
        normalDark: ("#8c8c8c")
        pressed: ("#0081FF")
        pressedDark: ("#0081FF")
    }
    D.ColorSelector.inactived: !surface.isActivated
    D.ColorSelector.hovered: false

    height: WaylandServer.config.windowTitlebarHeight
    width: surfaceItem.width

    // Ensure title bar does not accept keyboard focus
    focusPolicy: Qt.NoFocus

    HoverHandler {
        // block hover events to resizing mouse area, avoid cursor change
        cursorShape: Qt.ArrowCursor
        blocking: true
    }

    // One MouseArea owns every title bar press, so the ownership test below runs once per
    // press and a window move starts from the same grab. The TapHandler/DragHandler variant
    // could only gate its drag on the cursor position at drag time, and it was reverted after
    // the pointer wedged on real hardware; that cause is still unknown and is not addressed
    // by this file.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property point pressPosition
        property bool movingWindow: false
        // A press the window above covers reached this title bar only through a hole
        // in that window's input: do not activate or move this window on it.
        property bool pressBelongsToWindow: true
        // onPressAndHold cannot tell the device, Qt stamps that event as
        // MouseEventSynthesizedByQt for mice too. Real pointer events arrive as
        // NotSynthesized (WSeat sends them so, see waylib wseat.cpp), touch screens as
        // a synthesized press. Compared by equality since MouseEvent.source is
        // deprecated: a Qt build without it then only loses touch long press.
        property bool pressWasTouch: false

        onPressed: function (mouse) {
            pressPosition = Qt.point(mouse.x, mouse.y)
            movingWindow = false
            pressWasTouch = mouse.source === Qt.MouseEventSynthesizedByQt
                || mouse.source === Qt.MouseEventSynthesizedBySystem
            pressBelongsToWindow = root.surface.isTopmostWindowAt(
                root.mapToItem(null, Qt.point(mouse.x, mouse.y)))
        }
        onPositionChanged: function (mouse) {
            if (!pressed || movingWindow || !pressBelongsToWindow
                || !(mouse.buttons & Qt.LeftButton))
                return
            const dx = mouse.x - pressPosition.x
            const dy = mouse.y - pressPosition.y
            const threshold = Qt.styleHints.startDragDistance
            if (dx * dx + dy * dy < threshold * threshold)
                return
            movingWindow = true
            surface.moveRequested()
        }
        onClicked: function (mouse) {
            if (!pressBelongsToWindow || movingWindow)
                return
            if (mouse.button === Qt.LeftButton)
                WaylandServer.activateSurface(surface)
            else
                surface.windowMenuRequested(Qt.point(mouse.x, mouse.y))
        }
        onDoubleClicked: function (mouse) {
            if (pressBelongsToWindow && mouse.button === Qt.LeftButton && root.canToggleMaximize)
                surface.toggleMaximized()
        }
        onPressAndHold: function (mouse) {
            // Touch screen only, and never mid-drag: this timer is not stopped by
            // movement, while a dragged window keeps the cursor over the same spot.
            if (pressBelongsToWindow && pressWasTouch && !movingWindow)
                surface.windowMenuRequested(Qt.point(mouse.x, mouse.y))
        }
    }

    Rectangle {
        id: titlebar
        anchors.fill: parent
        color: root.D.ColorSelector.backgroundColor
        layer.enabled: !root.noRadius
        layer.smooth: !root.noRadius
        opacity: !root.noRadius ? 0 : parent.opacity
        Text {
            readonly property int spacenum: 4
            anchors {
                fill: parent
                leftMargin: spacenum * root.height
                rightMargin: spacenum * root.height
            }
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: surface.shellSurface.title
            elide: Text.ElideRight
            color: root.D.ColorSelector.textColor
            font.family: WaylandServer.config.font
            font.pointSize: WaylandServer.config.fontSize / 10
            font.weight: Font.Medium
        }

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
            }

            Loader {
                objectName: "minimizeBtn"
                sourceComponent: D.WindowButton {
                    icon.name: "window_minimize"
                    textColor: root.textColor
                    height: root.height
                    focusPolicy: Qt.NoFocus
                    D.ColorSelector.inactived: !surface.isActivated

                    onClicked: {
                        // A covered title bar can be reached through an area of the
                        // window above that owns no input; its buttons stay inert then.
                        if (!surface.isTopmostWindowUnderCursor())
                            return
                        surface.minimize()
                    }
                }
            }

            Loader {
                id: maxOrWindedBtn
                objectName: "maxOrWindedBtn"
                active: root.canToggleMaximize
                sourceComponent: D.WindowButton {
                    icon.name: surface.shellSurface.isMaximized ? "window_restore" : "window_maximize"
                    textColor: root.textColor
                    height: root.height
                    focusPolicy: Qt.NoFocus
                    D.ColorSelector.inactived: !surface.isActivated

                    onClicked: {
                        if (!surface.isTopmostWindowUnderCursor())
                            return
                        WaylandServer.activateSurface(surface)
                        surface.toggleMaximized()
                    }
                }
            }

            Loader {
                objectName: "closeBtn"
                sourceComponent: Item {
                    height: root.height
                    width: closeBtn.implicitWidth
                    Rectangle {
                        anchors.fill: closeBtn
                        color: closeBtn.hovered ? "red" : "transparent"
                    }
                    D.WindowButton {
                        id: closeBtn
                        icon.name: "window_close"
                        textColor: root.textColor
                        height: parent.height
                        focusPolicy: Qt.NoFocus
                        D.ColorSelector.inactived: !surface.isActivated

                        onClicked: {
                            if (!surface.isTopmostWindowUnderCursor())
                                return
                            surface.closeSurface()
                        }
                    }
                }
            }
        }
    }

    Loader {
        x: titlebar.x
        y: titlebar.y
        active: !root.noRadius
        sourceComponent: Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 0
                fillItem: titlebar
                PathRectangle {
                    width: titlebar.width
                    height: titlebar.height
                    topLeftRadius: surface.radius
                    topRightRadius: surface.radius
                }
            }
        }
    }
}
