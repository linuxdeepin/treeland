// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls

// PrelaunchSplash: minimalist standalone component with a white background and a centered logo.
// Created and managed by SurfaceWrapper; no window decorations (no border, no shadow).
Item {
    id: splash

    required property string logoPath
    required property real initialRadius
    property bool destroyAfterFade: false
    signal destroyRequested

    // Fill the entire parent (SurfaceWrapper)
    anchors.fill: parent

    Rectangle {
        id: background
        color: "#ffffff"
        anchors.fill: parent
        radius: initialRadius

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
        duration: 400

        onFinished: {
            splash.visible = false
            if (splash.destroyAfterFade) {
                // Request C++ side to destroy this item to avoid calling destroy()
                // on an object owned by C++.
                splash.destroyRequested();
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
