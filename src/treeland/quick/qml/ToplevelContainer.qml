// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import TreeLand
import TreeLand.Protocols
import TreeLand.Utils
import TreeLand.Protocols

Item {
    id: root
    required property int workspaceId
    required property int workspaceRelativeId
    anchors.fill: parent

    Button {
        onHoveredChanged: {
            console.log('allwins',root.children)
        }
        onClicked: {
            parent.parent.destroyWs(workspaceRelativeId)
        }
    }

    Component.onDestruction: {
        for(let item of children) {
            if(item instanceof XdgSurface)
                item.parent = QmlHelper.workspaces[0]
        }
    }
}