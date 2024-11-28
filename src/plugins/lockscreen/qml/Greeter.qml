// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Treeland
import Treeland.Greeter
import LockScreen

FocusScope {
    id: root
    clip: true
    enum CurrentMode {
        Lock = 1,
        Shutdown = 2,
        SwitchUser = 3
    }

    signal animationPlayed
    signal animationPlayFinished

    required property QtObject output
    required property QtObject outputItem
    property int currentMode: Greeter.CurrentMode.Lock

    function start()
    {
        wallpaperController.type = WallpaperController.Scale
        switch (root.currentMode) {
        case Greeter.CurrentMode.Lock:
            lockView.start()
            break;
        case Greeter.CurrentMode.SwitchUser:
            lockView.showUserView()
            break;
        }
    }

    x: outputItem.x
    y: outputItem.y
    width: outputItem.width
    height: outputItem.height

    WallpaperController {
        id: wallpaperController
        output: root.output
        lock: true
        type: WallpaperController.Normal
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
            PropertyAnimation {
                duration: 1000
                easing.type: Easing.OutExpo
            }
        }
    }

    LockView {
        id: lockView
        visible: root.currentMode === Greeter.CurrentMode.Lock ||
                 root.currentMode === Greeter.CurrentMode.SwitchUser
        anchors.fill: parent
        onQuit: function () {
            wallpaperController.type = WallpaperController.Normal
            root.animationPlayed()
        }
        onAnimationPlayFinished: function () {
            if (lockView.state === LoginAnimation.Hide) {
                root.animationPlayFinished()
            }
        }
    }

    ShutdownView {
        id: shutdownView
        visible: root.currentMode === Greeter.CurrentMode.Shutdown
        anchors.fill: parent

        onClicked: function () {
            root.animationPlayed()
            root.animationPlayFinished()
        }
        onSwitchUser: function () {
            root.currentMode = Greeter.CurrentMode.Lock
            lockView.showUserView()
        }
        onLock: function () {
            root.currentMode = Greeter.CurrentMode.Lock
            lockView.start()
        }
    }

    Component.onDestruction: {
        wallpaperController.lock = false
    }
}
