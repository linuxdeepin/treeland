// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import TestGlass

Item {
    id: root

    OutputRenderWindow {
        id: renderWindow
        objectName: "renderWindow"
        width: 256
        height: 256

        DynamicCreatorComponent {
            creator: TestHelper.outputCreator

            OutputItem {
                id: outputItem
                required property WaylandOutput waylandOutput
                output: waylandOutput
                devicePixelRatio: waylandOutput.scale

                OutputViewport {
                    id: outputViewport
                    output: waylandOutput
                    devicePixelRatio: parent.devicePixelRatio
                    anchors.centerIn: parent
                    width: 256
                    height: 256
                }

                GlassEffectScene {
                    id: scene
                    objectName: "glassScene"
                    anchors.centerIn: parent
                }
            }
        }
    }
}
