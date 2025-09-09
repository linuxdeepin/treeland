// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Treeland.Protocols

Item {
    id: root
    anchors.fill: parent

    property real scaleFactor: 1.0
    property var targetWindow: null

    Component.onCompleted: {
        targetWindow = parent && parent.Window ? parent.Window.window : Window.window
    }

    Rectangle {
        id: fpsDisplay
        width: Math.max(200 * scaleFactor, 180)
        height: Math.max(80 * scaleFactor, 70)
        color: "transparent"
        radius: 8 * scaleFactor
        border.color: "transparent"
        border.width: 0

        anchors {
            top: parent.top
            right: parent.right
            margins: 20 * scaleFactor
        }

        Component.onCompleted: {
            if (targetWindow && FpsDisplayManager) {
                FpsDisplayManager.setTargetWindow(targetWindow);
                FpsDisplayManager.start();
            }
        }

        Component.onDestruction: {
            if (targetWindow && FpsDisplayManager) {
                FpsDisplayManager.stop();
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Behavior on scale {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        Column {
            anchors.centerIn: parent
            spacing: 2 * scaleFactor

            Text {
                id: currentLabel
                text: qsTr("Current FPS: %1").arg(FpsDisplayManager.currentFps)
                color: "#000000"
                font.pixelSize: Math.max(16 * scaleFactor, 14)
                font.weight: Font.Medium
                font.family: "monospace"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
                style: Text.Raised
                styleColor: "#FFFFFF"
            }

            Text {
                id: maximumLabel
                text: qsTr("Maximum FPS: %1").arg(FpsDisplayManager.maximumFps)
                color: "#000000"
                font.pixelSize: Math.max(16 * scaleFactor, 14)
                font.weight: Font.Medium
                font.family: "monospace"
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
                style: Text.Raised
                styleColor: "#FFFFFF"
            }
        }
    }
}
