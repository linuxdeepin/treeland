// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls

// PrelaunchSplash: minimalist standalone component with a white background and a centered logo.
// Created and managed by SurfaceWrapper; no window decorations (no border, no shadow).
Item {
    id: splash

    property string logoPath: ""
    property bool destroyAfterFade: false

    // Fill the entire parent (SurfaceWrapper)
    anchors.fill: parent

    Rectangle {
        radius: 10 // TODO: use Decoration's radius
        id: background
        color: "#ffffff"
        // Fill parent; size is dictated by parent
        anchors.fill: parent

        // Centered logo
        Image {
            id: logoImage
            source: splash.logoPath
            width: 128
            height: 128
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            
            // Fallback if image doesn't exist
            Rectangle {
                anchors.fill: parent
                color: "#4CAF50"
                radius: 64
                visible: logoImage.status !== Image.Ready
                
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
    
    // Fade-out animation
    OpacityAnimator {
        id: fadeOut
        target: splash
        from: 1.0
        to: 0.0
        duration: 500

        onFinished: {
            splash.visible = false
            if (splash.destroyAfterFade) {
                splash.destroy();
            }
        }
    }

    function hide() {
        fadeOut.start()
    }

    function hideAndDestroy() {
        if (destroyAfterFade)
            return;
        destroyAfterFade = true;
        hide();
    }
}
