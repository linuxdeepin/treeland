// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Item {
    id: root
    anchors.fill: parent
    visible: WaylandServer.workspace.animationController.running
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Repeater {
        model: WaylandServer.rootContainer.outputModel
        Item {
            id: animationDelegate

            required property int index
            required property QtObject output
            readonly property real localAnimationScaleFactor: width / WaylandServer.workspace.animationController.refWidth
            clip: true
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height

            Row {
                x: - WaylandServer.workspace.animationController.viewportPos * animationDelegate.localAnimationScaleFactor
                spacing: WaylandServer.workspace.animationController.refGap * animationDelegate.localAnimationScaleFactor
                Repeater {
                    model: WaylandServer.workspace.models
                    delegate: Item {
                        width: animationDelegate.output.outputItem.width
                        height: animationDelegate.output.outputItem.height
                        id: workspaceDelegate
                        required property WorkspaceModel workspace
                        Wallpaper {
                            workspace: workspaceDelegate.workspace
                            output: animationDelegate.output.outputItem.output
                        }

                        WorkspaceProxy {
                            workspace: workspaceDelegate.workspace
                            output: animationDelegate.output
                        }
                    }
                }
            }
        }
    }
}
