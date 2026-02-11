// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Treeland
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
    property string primaryOutputName
    visible: primaryOutputName === "" || primaryOutputName === output.name

    function start(showAnimation)
    {
        if (showAnimation === undefined) {
            showAnimation = true
        }
        lockView.showAnimation = showAnimation
        lockView.forceActiveFocus()
        if (showAnimation) {
            Helper.startLockscreen(root.output, showAnimation);
            background.state = "Show"
            wallpaper.play = true;
        }
        background.opacity = 1.0
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

    palette.windowText: Qt.rgba(1.0, 1.0, 1.0, 1.0)

    Rectangle {
        id: background

        readonly property int duration: 1000
        anchors.fill: parent
        color: 'black'
        opacity: 0.0
        transformOrigin: Item.Center
        Behavior on opacity {
            PropertyAnimation {
                duration: lockView.showAnimation ? 2000 : 0
                easing.type: Easing.OutExpo
            }
        }
        Behavior on scale {
            PropertyAnimation {
                duration: lockView.showAnimation ? 2000 : 0
                easing.type: Easing.OutExpo
            }
        }
        states: [
            State {
                name: "Show"
                PropertyChanges {
                    target: background
                    scale: 1.2
                }
                PropertyChanges {
                    target: background
                    opacity: 1
                }
            },
            State {
                name: "Hide"
                PropertyChanges {
                    target: background
                    scale: 1
                }
                PropertyChanges {
                    target: background
                    opacity: 0
                }
            }
        ]

        transitions: [
            Transition {
                from: "Hide"
                to: "Show"
                PropertyAnimation {
                    property: "scale"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
            },
            Transition {
                from: "Show"
                to: "Hide"
                PropertyAnimation {
                    property: "scale"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
                PropertyAnimation {
                    property: "opacity"
                    duration: background.duration
                    easing.type: Easing.OutExpo
                }
            }
        ]
        Wallpaper {
            id: wallpaper

            anchors.fill: parent
            wallpaperRole: Wallpaper.Lockscreen
            output: root.output
            wallpaperState: Wallpaper.Normal
        }
    }

    // prevent event passing through greeter
    MouseArea {
        anchors.fill: parent
        enabled: true
    }

    LockView {
        id: lockView
        visible: root.currentMode === Greeter.CurrentMode.Lock ||
                 root.currentMode === Greeter.CurrentMode.SwitchUser
        anchors.fill: parent
        onQuit: function () {
            wallpaper.play = false;
            background.state = "Hide"
            root.animationPlayed()
            Helper.showDesktop(root.output)
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
}
