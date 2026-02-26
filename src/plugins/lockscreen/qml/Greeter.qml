// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Treeland
import LockScreen

FocusScope {
    id: root
    clip: true

    signal animationPlayed
    signal animationPlayFinished

    required property QtObject output
    required property QtObject outputItem
    property string primaryOutputName
    visible: primaryOutputName === "" || primaryOutputName === output.name

    x: outputItem.x
    y: outputItem.y
    width: outputItem.width
    height: outputItem.height

    palette.windowText: Qt.rgba(1.0, 1.0, 1.0, 1.0)

    /**************/
    /* Components */
    /**************/

    WallpaperController {
        id: wallpaperController
        output: root.output
        lock: true
        type: (GreeterProxy.isLocked || GreeterProxy.showShutdownView) ? WallpaperController.Scale : WallpaperController.Normal
    }

    // prevent event passing through greeter
    MouseArea {
        anchors.fill: parent
        enabled: true
    }

    Rectangle {
        id: cover
        anchors.fill: parent
        color: 'black'
        opacity: wallpaperController.type === WallpaperController.Normal ? 0 : 0.6
        Behavior on opacity {
            enabled: GreeterProxy.showAnimation
            PropertyAnimation {
                duration: 1000
                easing.type: Easing.OutExpo
            }
        }
    }

    LockView {
        id: lockView
        anchors.fill: parent
        onAnimationPlayFinished: function () {
            if (lockView.state === LoginAnimation.Hide) {
                root.animationPlayFinished()
            }
        }
    }

    ShutdownView {
        id: shutdownView
        visible: GreeterProxy.showShutdownView
        anchors.fill: parent

        onSwitchUser: {
            root.switchUser()
        }
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    function switchUser() {
        GreeterProxy.lock()
        lockView.showUserView()
    }

    Connections {
        target: GreeterProxy

        function onLockChanged(isLocked) {
            if (!isLocked)
                root.animationPlayed()
        }

        function onShowShutdownViewChanged(show) {
            if (!show && !GreeterProxy.isLocked) {
                root.animationPlayed()
                root.animationPlayFinished()
            }
        }

        function onSwitchUser() {
            root.switchUser()
        }
    }

    Component.onDestruction: {
        wallpaperController.lock = false
    }
}
