// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls

// PrelaunchSplash: 一个包含白色背景和居中Logo的极简自包含组件.
// 由 SurfaceWrapper 创建和管理，不包含窗口装饰（如边框、阴影）.
Item {
    id: splash

    property string logoPath: ""

    signal animationFinished() // 仍保留信号以兼容调用方

    // 填充父级 (SurfaceWrapper)
    anchors.fill: parent

    Rectangle {
        id: background
        color: "#ffffff"
        // 填充父级，尺寸由父级决定
        anchors.fill: parent

        // Logo 保持在背景中居中
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
        }
    }

    function hide() {
        fadeOut.start()
    }
}
