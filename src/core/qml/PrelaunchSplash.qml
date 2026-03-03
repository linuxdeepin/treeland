// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server 1.0

// Pre-launch splash QML item that can be shown before an application's main window appears.
Item {
    id: splash

    required property real initialRadius
    required property var iconBuffer
    required property color backgroundColor
    readonly property bool isLightBackground: backgroundColor.hslLightness >= 0.5
    signal destroyRequested

    // Fill the entire parent (SurfaceWrapper)
    anchors.fill: parent

    Rectangle {
        id: background
        color: splash.backgroundColor
        anchors.fill: parent
        radius: initialRadius

        // Centered logo: prefer provided icon buffer; fallback to image / placeholder
        Column {
            id: contentColumn
            anchors.centerIn: parent
            spacing: 12

            Item {
                id: logoContainer
                width: 88
                height: 88
                anchors.horizontalCenter: parent.horizontalCenter

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

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Loading application...")
                color: splash.isLightBackground ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(1, 1, 1, 0.5)
                font.pixelSize: 16 // TODO：use D.DTK.fontManager.t5
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
