// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
pragma Singleton

import QtQuick

import TreeLand.Greeter

Item {
    enum GreeterState {
        NotReady = 0,
        AuthSucceeded = 1,
        AuthFailed = 2,
        Quit = 3
    }

    property alias currentUser: userModel.currentUserName
    property int currentSession
    property var state: GreeterModel.NotRady
    readonly property UserModel userModel: userModel
    readonly property SessionModel sessionModel: sessionModel
    readonly property Proxy proxy: proxy
    readonly property LogoProvider logoProvider: logoProvider

    // TODO: use group to wait all animation
    signal animationPlayed
    signal animationPlayFinished

    function quit() {
        state = GreeterModel.Quit
    }

    Connections {
        target: userModel
        function onUpdateTranslations(locale) {
            console.log("translation updated")
            logoProvider.updateLocale(locale)
            TreeLand.retranslate()
        }
    }

    UserModel {
        id: userModel
    }

    SessionModel {
        id: sessionModel
    }

    Proxy {
        id: proxy
        sessionModel: sessionModel
        userModel: userModel

        function checkUser(userName) {
            let user = GreeterModel.userModel.get(GreeterModel.currentUser)
            console.log("last activate user:",user.name,"current user:",userName)
            return user.name === userName
        }

        onLoginSucceeded: function (userName) {
            if (!checkUser(userName)) {
                return
            }

            state = GreeterModel.AuthSucceeded
        }

        onLoginFailed: {
            if (!checkUser(userName)) {
                return
            }

            state = GreeterModel.AuthFailed
        }

        Component.onCompleted: {
            proxy.init()
        }
    }

    LogoProvider {
        id: logoProvider
    }

    Component.onCompleted: {
        GreeterModel.currentUser = userModel.data(userModel.index(userModel.lastIndex,0), UserModel.NameRole)
        GreeterModel.currentSession = sessionModel.lastIndex
    }
}
