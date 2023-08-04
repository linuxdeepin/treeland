pragma Singleton
import QtQuick

import TreeLand.Greeter

Item {
    property string currentUser
    property int currentSession
    readonly property UserModel userModel: userModel
    readonly property SessionModel sessionModel: sessionModel
    readonly property Proxy proxy: proxy

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
