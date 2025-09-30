// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls

Item {
    id: splash

    property string logoPath: ""
    property string appName: "正在启动应用..."

    signal animationFinished() // 仍保留信号以兼容调用方，但不再有尺寸过渡

    // 固定大小，不受父项影响
    anchors.fill: parent
    z: 999999
    
    Rectangle {
        id: content
        // 动态尺寸：根据内部列内容的 implicit 尺寸 + padding 计算，并保持最小尺寸
        property int paddingH: 40
        property int paddingV: 32
        property int minContentSize: 260
        // 计算隐式尺寸（列内容 + padding）
        // implicitWidth: Math.max(minContentSize, columnContent.implicitWidth + paddingH)
        // implicitHeight: Math.max(minContentSize, columnContent.implicitHeight + paddingV)
        // width: implicitWidth
        // height: implicitHeight
        anchors.fill: parent
        color: "#ffffff"
        radius: 24
        // 始终居中
        //anchors.centerIn: parent
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
            id: columnContent
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
                wrapMode: Text.WrapAnywhere
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
                // 限制单行无限拉伸，宽度不超过某个阈值（与最小/最大策略一致）
                // 如果需要可改成基于父 width 的比例
                maximumLineCount: 3
            }
        }
    }
    
    // 去掉尺寸变化动画，仅保留淡出

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

    // 兼容旧调用：现在不再执行大小过渡，直接触发完成并淡出
    function animateToSize(targetWidth, targetHeight) {
        // 若调用方仍传入比默认大/小的尺寸，这里忽略尺寸调整，仅保持稳定视觉
        splash.animationFinished()
        fadeOut.start()
    }
}
