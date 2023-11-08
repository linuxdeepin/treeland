// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

InputPopupSurfaceItem {
    required property WaylandInputPopupSurface waylandSurface

    surface: waylandSurface

    x: waylandSurface.pos.x
    y: waylandSurface.pos.y

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
