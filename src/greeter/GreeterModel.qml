// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton
import QtQuick

import TreeLand.Greeter

Item {
    property string currentUser
    property int currentSession
    readonly property UserModel userModel: userModel
    readonly property SessionModel sessionModel: sessionModel
    readonly property Proxy proxy: proxy

    // TODO: use group to wait all animation
    signal animationPlayed()
    signal animationPlayFinished()

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
    }

    Component.onCompleted: {
        GreeterModel.currentUser = userModel.lastUser
        GreeterModel.currentSession = sessionModel.lastIndex
    }
}
