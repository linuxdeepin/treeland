// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server
import TreeLand

Item {
    id: root
    required property ListModel model
    required property OutputRenderWindow outputRenderWindow

    onVisibleChanged: {
        console.log('calc',grid.calcLayout())
    }

    EQHGrid {
        id: grid
        anchors.centerIn: parent
        model: root.model
        minH: 100
        maxH: parent.height
        maxW: parent.width
        availH: parent.height
        availW: parent.width
        spacing: 20
        delegate: Item {
                property XdgSurface source: modelData.source
                width: modelData.dw
                height: width * source.height / source.width
                clip: true
                property bool highlighted: hvhdlr.hovered
                HoverHandler {
                    id: hvhdlr
                }
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.width: highlighted ? 2 : 0
                    border.color: "blue"
                    radius: 8
                }
                ShaderEffectSource {
                    anchors {
                        fill: parent
                        margins: 3
                    }
                    live: true
                    hideSource: false
                    smooth: true
                    sourceItem: source
                }
                Control {
                    id: titleBox
                    anchors {
                        bottom: parent.bottom
                        horizontalCenter: parent.horizontalCenter
                        margins: 10
                    }
                    width: Math.min(implicitContentWidth + 2 * padding,parent.width*.7)
                    padding: 10
                    visible: highlighted
                    
                    contentItem: Text {
                        text: source.waylandSurface.title
                        elide: Qt.ElideRight
                    }
                    background: Rectangle {
                        color: Qt.rgba(255,255,255,.2)
                        radius: 5
                    }
                }
            }
    }
}
