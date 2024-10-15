// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server

OutputItem {
    id: outputItem

    required property PrimaryOutput targetOutputItem
    property OutputViewport screenViewport: targetOutputItem.screenViewport

    devicePixelRatio: output?.scale ?? devicePixelRatio

    Rectangle {
        id: content
        anchors.fill: parent
        color: "gray"

        TextureProxy {
            id: proxy
            sourceItem: screenViewport
            anchors.centerIn: parent
            rotation: targetOutputItem.keepAllOutputRotation ? 0 : screenViewport.rotation
            width: screenViewport.implicitWidth
            height: screenViewport.implicitHeight
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
            color: "red"
        }
    }

    OutputViewport {
        id: viewport

        anchors.centerIn: parent
        depends: [screenViewport]
        devicePixelRatio: outputItem.devicePixelRatio
        input: content
        output: outputItem.output
        ignoreViewport: true
    }
}
