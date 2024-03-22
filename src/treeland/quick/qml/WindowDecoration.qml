// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Qt5Compat.GraphicalEffects
import QtQuick.Controls
import Waylib.Server
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS
import TreeLand.Utils

D.RoundRectangle {
    id: root
    radius: 15
    corners: D.RoundRectangle.TopLeftCorner | D.RoundRectangle.TopRightCorner
    color: "transparent"

    signal requestMove
    signal requestMinimize
    signal requestToggleMaximize(var max)
    signal requestClose
    signal requestResize(var edges)

    readonly property real topMargin: titlebar.height
    readonly property real bottomMargin: 0
    readonly property real leftMargin: 0
    readonly property real rightMargin: 0
    property D.Palette backgroundColor: DS.Style.highlightPanel.background
    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow
    property D.Palette innerShadowColor: DS.Style.highlightPanel.innerShadow


    required property ToplevelSurface surface

    D.BoxShadow {
        anchors.fill: root
        shadowColor: root.D.ColorSelector.outerShadowColor
        shadowOffsetY: 4
        shadowBlur: 16
        cornerRadius: root.radius
        hollow: true
    }


    MouseArea {
        property int edges: 0

        anchors {
            fill: parent
            margins: -5
        }

        hoverEnabled: true
        Cursor.shape: {
            switch(edges) {
            case Qt.TopEdge:
                return Cursor.TopSide
            case Qt.RightEdge:
                return Cursor.RightSide
            case Qt.BottomEdge:
                return Cursor.BottomSide
            case Qt.LeftEdge:
                return Cursor.LeftSide
            case Qt.TopEdge | Qt.LeftEdge:
                return Cursor.TopLeftCorner
            case Qt.TopEdge | Qt.RightEdge:
                return Cursor.TopRightCorner
            case Qt.BottomEdge | Qt.LeftEdge:
                return Cursor.BottomLeftCorner
            case Qt.BottomEdge | Qt.RightEdge:
                return Cursor.BottomRightCorner
            }

            return Qt.ArrowCursor;
        }

        onPositionChanged: function (event) {
            edges = WaylibHelper.getEdges(Qt.rect(0, 0, width, height), Qt.point(event.x, event.y), 10)
        }

        onPressed: {
            root.requestResize(edges)
            Helper.activatedSurface = surface
        }
    }

    D.RoundRectangle {
        id: titlebar
        anchors.top: parent.top
        width: parent.width
        height: 30
        radius: parent.radius
        corners: parent.corners
    }
    // TODO: use twice element, need optimize.
    Rectangle {
        anchors.fill: titlebar
        layer.enabled: true
        layer.effect: OpacityMask {
            maskSource: titlebar
        }

        MouseArea {
            anchors.fill: parent
            Cursor.shape: pressed ? Cursor.Grabbing : Qt.ArrowCursor

            onPressed: {
                Helper.activatedSurface = surface
                root.requestMove()
            }
        }

        Label {
            anchors.centerIn: parent
            clip: true
            text: surface.title
        }
        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
            }

            Item {
                id: control
                property D.Palette textColor: DS.Style.button.text
                property D.Palette backgroundColor: DS.Style.windowButton.background
            }

            Loader {
                objectName: "minimizeBtn"
                sourceComponent: D.WindowButton {
                    icon.name: "window_minimize"
                    textColor: control.textColor
                    height: titlebar.height

                    onClicked: {
                        root.requestMinimize()
                    }
                }
            }

            Loader {
                objectName: "quitFullBtn"
                visible: false
                sourceComponent: D.WindowButton {
                    icon.name: "window_quit_full"
                    textColor: control.textColor
                    height: titlebar.height

                    onClicked: {
                    }
                }
            }

            Loader {
                id: maxOrWindedBtn
                objectName: "maxOrWindedBtn"
                sourceComponent: D.WindowButton {
                    icon.name: surface.isMaximized ? "window_restore" : "window_maximize"
                    textColor: control.textColor
                    height: titlebar.height

                    onClicked: {
                        root.requestToggleMaximize(!surface.isMaximized)
                    }
                }
            }

            Loader {
                objectName: "closeBtn"
                sourceComponent: D.WindowButton {
                    icon.name: "window_close"
                    textColor: control.textColor
                    height: titlebar.height

                    onClicked: {
                        root.requestClose()
                    }
                }
            }
        }
    }
}
