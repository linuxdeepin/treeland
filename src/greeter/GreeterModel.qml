pragma Singleton
import QtQuick

import TreeLand.Greeter

Item {
    property string currentUser
    property int currentSession
    readonly property UserModel userModel: userModel
    readonly property SessionModel sessionModel: sessionModel
    readonly property Worker worker: worker

    UserModel {
        id: userModel
    }

    SessionModel {
        id: sessionModel
    }

    Worker {
        id: worker
        proxy: proxy
        sessionModel: sessionModel
        userModel: userModel
    }

    Connections {
        target: worker
        function onFramworkStateChanged(v) {
            console.log(v)
        }
    }

    Component.onCompleted: {
        GreeterModel.currentUser = userModel.lastUser
        GreeterModel.currentSession = sessionModel.lastIndex
    }
}
