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

    height: Helper.config.windowTitlebarHeight
    width: surfaceItem.width

    // Ensure title bar does not accept keyboard focus
    focusPolicy: Qt.NoFocus

    HoverHandler {
        // block hover events to resizing mouse area, avoid cursor change
        cursorShape: Qt.ArrowCursor
        blocking: true
    }

    // Left mouse button handler
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            Helper.activateSurface(surface)
        }
        onPressedChanged: {
            if (pressed)
                surface.requestMove()
        }
        onDoubleTapped: {
            if (root.canToggleMaximize)
                surface.requestToggleMaximize()
        }
    }

    // Right mouse button handler
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: {
            surface.requestShowWindowMenu(eventPoint.position)
        }
    }

    // Touch screen click
    TapHandler {
        acceptedButtons: Qt.NoButton
        acceptedDevices: PointerDevice.TouchScreen
        onDoubleTapped: {
            if (root.canToggleMaximize)
                surface.requestToggleMaximize()
        }
        onLongPressed: surface.requestShowWindowMenu(point.position)
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
            font.family: Helper.config.font
            font.pointSize: Helper.config.fontSize / 10
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
                        surface.requestMinimize()
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
                        Helper.activateSurface(surface)
                        surface.requestToggleMaximize()
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
                            surface.requestClose()
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
