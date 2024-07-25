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
    }

    // prevent event passing through greeter
    MouseArea {
        anchors.fill: parent
        enabled: true
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

    function checkUser(userName) {
        let user = GreeterModel.userModel.get(GreeterModel.currentUser)
        console.log("last activate user:",user.name,"current user:",userName)
        return user.name === userName
    }

    Connections {
        target: GreeterModel.proxy
        function onLoginSucceeded(userName) {
            if (!checkUser(userName)) {
                return
            }

            center.loginGroup.userAuthSuccessed()
            center.loginGroup.updateHintMsg(center.loginGroup.normalHint)
            GreeterModel.emitAnimationPlayed()
        }
    }

    Connections {
        target: GreeterModel.proxy
        function onLoginFailed(userName) {
            if (!checkUser(userName)) {
                return
            }

            center.loginGroup.userAuthFailed()
            center.loginGroup.updateHintMsg(qsTr("Password is incorrect."))
        }
    }

    Connections {
        target: GreeterModel
        function onAnimationPlayed() {
            wallpaperController.type = Helper.Normal
        }
    }

    Component.onCompleted: {
        wallpaperController.type = Helper.Scale
    }
}
