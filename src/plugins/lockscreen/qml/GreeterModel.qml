// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
pragma Singleton

import QtQuick
import Treeland.Greeter
import LockScreen

Item {
    enum GreeterState {
        NotReady = 0,
        AuthSucceeded = 1,
        AuthFailed = 2,
        Quit = 3
    }

    readonly property var currentUser: UserModel.currentUserName
    property int currentSession
    property var state: GreeterModel.NotRady
    readonly property SessionModel sessionModel: SessionModel
    readonly property GreeterProxy proxy: proxy
    readonly property LogoProvider logoProvider: logoProvider

    function quit() {
        state = GreeterModel.Quit
    }

    Connections {
        target: UserModel
        function onUpdateTranslations(locale) {
            console.log("translation updated")
            logoProvider.updateLocale(locale)
            Treeland.retranslate()
        }
    }

    GreeterProxy {
        id: proxy
        sessionModel: SessionModel
        userModel: UserModel

        function checkUser(userName) {
            let user = UserModel.get(UserModel.currentUserName)
            console.log("last activate user:",user.name,"current user:",userName)
            return user.name === userName
        }

        onLoginSucceeded: function(userName) {
            if (!checkUser(userName)) {
                return
            }

            state = GreeterModel.AuthSucceeded
        }

        onLoginFailed: function(userName) {
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
        UserModel.currentUserName = UserModel.data(UserModel.index(UserModel.lastIndex,0), UserModel.NameRole)
        GreeterModel.currentSession = SessionModel.lastIndex
    }
}
