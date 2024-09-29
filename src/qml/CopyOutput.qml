// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

OutputItem {
    id: outputItem

    required property OutputItem targetOutputItem
    required property OutputViewport targetViewport

    devicePixelRatio: output?.scale ?? devicePixelRatio

    Rectangle {
        id: content
        anchors.fill: parent
        color: "gray"

        TextureProxy {
            id: proxy
            sourceItem: targetViewport
            anchors.centerIn: parent
            rotation: targetOutputItem.keepAllOutputRotation ? 0 : targetViewport.rotation
            width: targetViewport.implicitWidth
            height: targetViewport.implicitHeight
            smooth: true
            transformOrigin: Item.Center
            scale: {
                const isize = targetOutputItem.keepAllOutputRotation
                            ? Qt.size(width, height)
                            : Qt.size(targetOutputItem.width, targetOutputItem.height);
                const osize = Qt.size(outputItem.width, outputItem.height);
                const size = WaylibHelper.scaleSize(isize, osize, Qt.KeepAspectRatio);
                return size.width / isize.width;
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "I'm a duplicate of the primary screen"
            font.pointSize: 18
            color: "yellow"
        }
    }

    OutputViewport {
        id: viewport

        anchors.centerIn: parent
        depends: [targetViewport]
        devicePixelRatio: outputItem.devicePixelRatio
        input: content
        output: outputItem.output
        ignoreViewport: true
    }
}
