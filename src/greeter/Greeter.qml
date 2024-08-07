// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import TreeLand.Greeter
import TreeLand.Utils

FocusScope {
    id: root
    clip: true

    required property var output

    WallpaperController {
        id: wallpaperController
        output: root.output
        lock: true
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
        opacity: 0.0
        Behavior on opacity {
            PropertyAnimation {
                duration: 1000
            }
        }
    }

    Center {
        id: center
        anchors.fill: parent
        anchors.leftMargin: 50
        anchors.topMargin: 50
        anchors.rightMargin: 50
        anchors.bottomMargin: 50

        focus: true
    }

    Connections {
        target: GreeterModel
        function onStateChanged() {
            switch (GreeterModel.state) {
                case GreeterModel.AuthSucceeded: {
                    center.loginGroup.userAuthSuccessed()
                    center.loginGroup.updateHintMsg(center.loginGroup.normalHint)
                    GreeterModel.quit()
                }
                break
                case GreeterModel.AuthFailed: {
                    center.loginGroup.userAuthFailed()
                    center.loginGroup.updateHintMsg(qsTr("Password is incorrect."))
                }
                break
                case GreeterModel.Quit: {
                    GreeterModel.emitAnimationPlayed()
                    wallpaperController.type = Helper.Normal
                    cover.opacity = 0.0
                }
                break
            }
        }
    }

    Component.onCompleted: {
        wallpaperController.type = Helper.Scale
        cover.opacity = 0.4
    }

    Component.onDestruction: {
        wallpaperController.lock = false
    }
}
