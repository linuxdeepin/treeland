// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import TreeLand.Greeter

Item {
    id: root
    // prevent event passing through greeter
    MouseArea {
        anchors.fill: parent
        enabled: true
    }

    Image {
        id: background
        source: "file:///usr/share/wallpapers/deepin/desktop.jpg"
        fillMode: Image.PreserveAspectCrop
        anchors.fill: parent
        ScaleAnimator on scale {
            from: 1
            to: 1.1
            duration: 400
        }
    }

    ParallelAnimation {
        id: backgroundAni
        ScaleAnimator {
            target: background
            from: 1.1
            to: 1
            duration: 400
        }
        PropertyAnimation {
            target: background
            property: "opacity"
            duration: 600
            from: 1
            to: 0
            easing.type: Easing.InQuad
        }
    }

    Center {
        anchors.fill: parent
        anchors.leftMargin: 50
        anchors.topMargin: 50
        anchors.rightMargin: 50
        anchors.bottomMargin: 50
    }

    Connections {
        target: GreeterModel.proxy
        function onLoginSucceeded(userName) {
            var authUser = GreeterModel.userModel.lastUser;
            if(authUser.length != 0 && authUser != userName) {
                return
            }

           GreeterModel.emitAnimationPlayed()
        }
    }

    Connections {
        target: GreeterModel.proxy
        function onLoginFailed(user) {
            if(GreeterModel.userModel.lastUser() != userName){
                return
            }

            console.log("login failed:",user)
        }
    }

    Connections {
        target: GreeterModel
        function onAnimationPlayed() {
            backgroundAni.start()
        }
        function onAnimationPlayFinished() {
            visible = false

            background.scale = 1.1
            background.opacity = 1
        }
    }
}
