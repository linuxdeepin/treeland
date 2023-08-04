// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

XdgSurfaceItem {
    required property WaylandXdgSurface waylandSurface
    property string type

    surface: waylandSurface

    OutputLayoutItem {
        anchors.fill: parent
        layout: QmlHelper.layout

        onEnterOutput: function(output) {
            waylandSurface.surface.enterOutput(output);
        }
        onLeaveOutput: function(output) {
            waylandSurface.surface.leaveOutput(output);
        }
    }
}
