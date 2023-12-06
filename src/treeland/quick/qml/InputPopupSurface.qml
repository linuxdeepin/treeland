// Copyright (C) 2023 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

InputPopupSurfaceItem {
    required property InputMethodHelper helper

    property var cursorPoint: helper.activeFocusItem.mapToItem(parent, Qt.point(helper.cursorRect.x, helper.cursorRect.y))
    property real cursorWidth: helper.cursorRect.width
    property real cursorHeight: helper.cursorRect.height

    x: {
        var x = cursorPoint.x + cursorWidth
        if (x + width > parent.width) {
            x = parent.width - width
        }
        return x
    }

    y: {
        var y = cursorPoint.y + cursorHeight
        if (y + height > parent.height) {
            y = min(cursorPoint.y - height, parent.height - height)
        }
        return y
    }

    OutputLayoutItem {
        anchors.fill: parent
        layout: QmlHelper.layout

        onEnterOutput: function(output) {
            surface.surface.enterOutput(output);
        }
        onLeaveOutput: function(output) {
            surface.surface.leaveOutput(output);
        }
    }
}
