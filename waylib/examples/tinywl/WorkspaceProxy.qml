// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Tinywl

Item {
    required property WorkspaceModel workspace
    required property QtObject output

    width: output.outputItem.width
    height: output.outputItem.height
    clip: true

    Repeater {
        model: workspace
        delegate: Loader {
            id: loader

            required property SurfaceWrapper surface
            required property int orderIndex

            x: surface.x - output.outputItem.x
            y: surface.y - output.outputItem.y
            z: orderIndex
            active: surface.ownsOutput === output
                    && surface.surfaceState !== SurfaceWrapper.State.Minimized
            sourceComponent: SurfaceProxy {
                surface: loader.surface
                fullProxy: true
            }
        }
    }

    Repeater {
        model: Helper.workspace.showOnAllWorkspaceModel
        delegate: Loader {
            id: loader

            required property SurfaceWrapper surface
            required property int orderIndex

            x: surface.x - output.outputItem.x
            y: surface.y - output.outputItem.y
            z: orderIndex
            active: surface.ownsOutput === output
                    && surface.surfaceState !== SurfaceWrapper.State.Minimized
            sourceComponent: SurfaceProxy {
                surface: loader.surface
                fullProxy: true
            }
        }
    }
}
