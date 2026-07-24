// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

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

    required property QtObject output // Treeland Output (QML_ANONYMOUS), cannot use as named type
    required property Item outputItem
    property string primaryOutputName
    property bool isPrimaryOutput: primaryOutputName === "" || primaryOutputName === output.name
    visible: true

    x: outputItem.x
    y: outputItem.y
    width: outputItem.width
    height: outputItem.height

    palette.windowText: Qt.rgba(1.0, 1.0, 1.0, 1.0)

    /**************/
    /* Components */
    /**************/

    Rectangle {
        id: background

        readonly property int duration: 1000
        anchors.fill: parent
        clip: true
        color: 'black'
        opacity: 0.0
        transformOrigin: Item.Center
        state: (GreeterProxy.isLocked || GreeterProxy.showShutdownView) ? "Show" : "Hide"
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
                from: "*"
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
                from: "*"
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
        onStateChanged: {
            if (state === "Show") {
                Helper.startLockscreen(root.output, true);
                wallpaper.play = true;
            } else {
                wallpaper.play = false;
                Helper.showDesktop(root.output)
            }
        }

        Wallpaper {
            id: wallpaper

            anchors.fill: parent
            clip: true
            wallpaperRole: Wallpaper.Lockscreen
            output: root.output
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: true
    }

    Loader {
        id: lockViewLoader
        anchors.fill: parent
        active: isPrimaryOutput
        sourceComponent: lockViewComponent
    }

    Component {
        id: lockViewComponent
        LockView {
            // qmllint disable unqualified: qmllint directive — root is outer scope
            id: lockView
            anchors.fill: parent
            onAnimationPlayFinished: function () {
                if (lockView.state === LoginAnimation.Hide) {
                    root.animationPlayFinished()
                }
            }
            // qmllint enable unqualified
        }
    }

    Loader {
        id: shutdownViewLoader
        anchors.fill: parent
        active: GreeterProxy.showShutdownView && isPrimaryOutput
        sourceComponent: shutdownViewComponent
    }

    Component {
        id: shutdownViewComponent
        ShutdownView {
            // qmllint disable unqualified: qmllint directive — root is outer scope
            id: shutdownView
            anchors.fill: parent
            onSwitchUser: {
                root.switchUser()
            }
            // qmllint enable unqualified
        }
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    function switchUser(): void {
        GreeterProxy.lock()
        lockView.showUserView()
    }

    Connections {
        target: GreeterProxy

        function onLockChanged(isLocked: bool) {
            if (!isLocked)
                root.animationPlayed()
        }

        function onShowShutdownViewChanged(show: bool) {
            if (!show && !GreeterProxy.isLocked) {
                root.animationPlayed()
                root.animationPlayFinished()
            }
        }

        function onSwitchUser() {
            root.switchUser()
        }
    }
}
