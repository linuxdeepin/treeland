// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Tinywl

Item {
    id: root

    required property WorkspaceModel from
    required property WorkspaceModel to
    readonly property Item workspace: parent
    readonly property WorkspaceModel leftWorkspace: {
        if (!from || !to)
            return null;
        return from.index > to.index ? to : from;
    }
    readonly property WorkspaceModel rightWorkspace: {
        if (!from || !to)
            return null;
        return from.index > to.index ? from : to;
    }
    property int duration: 200 * Helper.animationSpeed

    anchors.fill: parent
    z: 1

    Repeater {
        model: workspace.root.outputModel
        delegate: Item {
            id: rootItem

            required property QtObject output
            readonly property PrimaryOutput outputItem: output.outputItem

            x: outputItem.x
            y: outputItem.y
            width: outputItem.width
            height: outputItem.height

            Row {
                id: wallpapers
                spacing: workspaces.spacing
                x: workspaces.x
                parent: rootItem.outputItem.parent
                z: rootItem.outputItem.z - 1

                Wallpaper {
                    userId: Helper.currentUserId
                    output: rootItem.outputItem.output
                    workspace: root.leftWorkspace
                    width: rootItem.outputItem.width
                    height: rootItem.outputItem.height
                }

                Wallpaper {
                    userId: Helper.currentUserId
                    output: rootItem.outputItem.output
                    workspace: root.rightWorkspace
                    width: rootItem.outputItem.width
                    height: rootItem.outputItem.height
                }
            }

            Row {
                id: workspaces

                spacing: 30

                WorkspaceProxy {
                    workspace: root.leftWorkspace
                    output: rootItem.output
                }

                WorkspaceProxy {
                    workspace: root.rightWorkspace
                    output: rootItem.output
                }
            }

            ParallelAnimation {
                id: animation

                XAnimator {
                    id: wallpapersAnimation
                    target: wallpapers
                    duration: root.duration
                }

                XAnimator {
                    id: workspacesAnimation
                    target: workspaces
                    duration: root.duration
                }

                onFinished: {
                    rootItem.outputItem.wallpaperVisible = true;
                    Helper.workspace.showOnAllWorkspaceModel.visible = true;
                    root.workspace.current = root.to;
                }
            }

            Component.onCompleted: {
                if (root.from === root.leftWorkspace) {
                    wallpapersAnimation.from = 0;
                    wallpapersAnimation.to = -workspaces.width + outputItem.width;
                } else {
                    wallpapersAnimation.from = -workspaces.width + outputItem.width;
                    wallpapersAnimation.to = 0;
                }

                workspacesAnimation.from = wallpapersAnimation.from;
                workspacesAnimation.to = wallpapersAnimation.to;

                rootItem.outputItem.wallpaperVisible = false;
                animation.start();
            }
        }
    }
}
