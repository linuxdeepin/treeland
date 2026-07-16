// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import Treeland

Item {
    id: root
    required property WorkspaceModel workspace
    required property QtObject output // Treeland Output (QML_ANONYMOUS), cannot use as named type

    width: output.outputItem.width
    height: output.outputItem.height
    clip: true

    Repeater {
        model: workspace
        delegate: Loader {
            id: loader

            required property SurfaceWrapper surface
            required property int orderIndex

            // qmllint disable unqualified: qmllint directive — output is an outer scope property
            x: surface.x - output.outputItem.x
            y: surface.y - output.outputItem.y
            z: orderIndex
            active: surface.ownsOutput === output
                    && surface.surfaceState !== SurfaceWrapper.State.Minimized
            // qmllint enable unqualified
            sourceComponent: Component {
                SurfaceProxy {
                    fullProxy: true
                }
            }
            onLoaded: {
                if (item)
                    item.surface = Qt.binding(() => loader.surface)
            }
        }
    }
    Repeater {
        model: Helper.workspace.showOnAllWorkspaceModel
        delegate: Loader {
            id: allLoader

            required property SurfaceWrapper surface
            required property int orderIndex

            // qmllint disable unqualified: qmllint directive — output is an outer scope property
            x: surface.x - output.outputItem.x
            y: surface.y - output.outputItem.y
            z: orderIndex
            active: surface.ownsOutput === output
                    && surface.surfaceState !== SurfaceWrapper.State.Minimized
            // qmllint enable unqualified
            sourceComponent: Component {
                SurfaceProxy {
                    fullProxy: true
                }
            }
            onLoaded: {
                if (item)
                    item.surface = Qt.binding(() => allLoader.surface)
            }
        }
    }
}
