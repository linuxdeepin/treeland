// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls

Item {
    id: splash

    property string logoPath: "/usr/share/icons/bloom/128x128/apps/browser360-cn.png"
    property string appName: "正在启动应用..."

    signal animationFinished()

    // 固定大小，不受父项影响
    anchors.centerIn: parent
    width: 320  // 固定宽度，比content稍大一些
    height: 320 // 固定高度，比content稍大一些
    z: 999999
    
    Rectangle {
        id: content
        width: 280
        height: 280
        color: "#ffffff"
        radius: 24
        // 始终居中
        anchors.centerIn: parent
        border.color: "#e0e0e0"
        border.width: 1
        
        // Drop shadow effect
        Rectangle {
            id: shadow
            width: parent.width + 8
            height: parent.height + 8
            color: "#20000000"
            radius: parent.radius + 4
            anchors.centerIn: parent
            z: -1
        }
        
        Column {
            anchors.centerIn: parent
            spacing: 20
            
            Image {
                id: logoImage
                source: splash.logoPath
                width: 128
                height: 128
                anchors.horizontalCenter: parent.horizontalCenter
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
            
            Text {
                text: splash.appName
                color: "#666666"
                font.pixelSize: 16
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
    
    // Size transition animation
    ParallelAnimation {
        id: sizeAnimation

        PropertyAnimation {
            id: widthAnimation
            target: content
            property: "width"
            from: content.width
            duration: 800
            easing.type: Easing.InOutQuart
        }

        PropertyAnimation {
            id: heightAnimation
            target: content
            property: "height"
            from: content.height
            duration: 800
            easing.type: Easing.InOutQuart
        }

        onFinished: {
            // 大小动画完成后，发出信号并开始淡出动画
            splash.animationFinished()
            fadeOut.start()
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

    function animateToSize(targetWidth, targetHeight) {
        console.log("animateToSize called with:", targetWidth, targetHeight)
        console.log("current content size:", content.width, content.height)

        // 确保动画从当前大小开始
        widthAnimation.from = content.width
        heightAnimation.from = content.height

        // 设置目标大小
        var finalWidth = Math.max(targetWidth, 280)
        var finalHeight = Math.max(targetHeight, 280)
        widthAnimation.to = finalWidth
        heightAnimation.to = finalHeight

        console.log("animating from", content.width, content.height, "to", finalWidth, finalHeight)
        sizeAnimation.start()
        console.log("animation started")
    }
}
