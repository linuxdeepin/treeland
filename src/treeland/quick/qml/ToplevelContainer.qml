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

    Item {
        id: dbg
        anchors {
            top: parent.top
            right: parent.right
        }
        width: 200
        Column {
            Text {
                text: `No.${workspaceRelativeId} wsid: ${workspaceId}`
                color: "white"
            }
        }
    }

    Component.onDestruction: {
        const moveToRelId = workspaceRelativeId > 0 ? workspaceRelativeId - 1 : workspaceRelativeId + 1
        const moveTo = QmlHelper.workspaceManager.layoutOrder.get(moveToRelId).wsid
        console.log(`workspace ${workspaceId} onDestruction, move wins to (No.${moveToRelId}, id=${moveTo})`)
        for(let item of children) {
            if(item instanceof XdgSurface)
                item.workspaceId = moveTo
        }
    }
}