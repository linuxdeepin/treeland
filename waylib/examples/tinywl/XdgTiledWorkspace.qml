// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

Item {
    property DynamicCreator creator

    Text {
        anchors.centerIn: parent
        text: "TODO"
        font.pointSize: 55
        color: "red"
    }
}
