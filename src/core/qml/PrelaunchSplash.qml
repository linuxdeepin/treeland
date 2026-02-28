// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server 1.0

// Pre-launch splash QML item that can be shown before an application's main window appears.
Item {
    id: splash

    required property real initialRadius
    property var iconBuffer
    required property color backgroundColor
    signal destroyRequested

    // Fill the entire parent (SurfaceWrapper)
    anchors.fill: parent

    Rectangle {
        id: background
        color: splash.backgroundColor
        anchors.fill: parent
        radius: initialRadius

        // Centered logo: prefer provided icon buffer; fallback to image / placeholder
        Item {
            id: logoContainer
            width: 128
            height: 128
            anchors.centerIn: parent

            BufferItem {
                anchors.fill: parent
                visible: !!splash.iconBuffer
                buffer: splash.iconBuffer
                smooth: true
            }

            // Fallback placeholder when no icon buffer is provided
            Rectangle {
                anchors.fill: parent
                visible: !splash.iconBuffer
                color: "#4CAF50"
                radius: width / 2

                Text {
                    anchors.centerIn: parent
                    text: "App"
                    font.pixelSize: 24
                    color: "white"
                    font.bold: true
                }
            }
        }
    }

    function hideAndDestroy() {
        if (!splash.visible) {
            console.warn("PrelaunchSplash: Already hidden, ignoring hideAndDestroy call.");
            return;
        }
        splash.visible = false
        splash.destroyRequested();
    }
}
