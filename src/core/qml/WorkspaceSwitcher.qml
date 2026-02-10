// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Treeland

Item {
    id: root
    anchors.fill: parent
    visible: Helper.workspace.animationController.running
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Repeater {
        model: Helper.rootContainer.outputModel
        Item {
            id: animationDelegate

            required property int index
            required property QtObject output
            readonly property real localAnimationScaleFactor: width / Helper.workspace.animationController.refWidth
            clip: true
            x: output.outputItem.x
            y: output.outputItem.y
            width: output.outputItem.width
            height: output.outputItem.height

            Row {
                x: - Helper.workspace.animationController.viewportPos * animationDelegate.localAnimationScaleFactor
                spacing: Helper.workspace.animationController.refGap * animationDelegate.localAnimationScaleFactor
                Repeater {
                    model: Helper.workspace.models
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
