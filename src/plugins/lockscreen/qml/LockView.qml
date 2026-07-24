// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Treeland

FocusScope {
    id: root
    property int state: LoginAnimation.Show
    readonly property bool powerVisible: controlAction.powerVisible
    signal animationPlayFinished()

    /**************/
    /* Components */
    /**************/

    function show(): void {
        root.forceActiveFocus()
        root.visible = true
        root.state = LoginAnimation.Show
        leftAnimation.item.start({x: root.x - quickAction.width, y: quickAction.y}, {x: quickAction.x, y: quickAction.y})
        logoAnimation.item.start({x: root.x - logo.width, y: logo.y}, {x: logo.x, y: logo.y})
        rightAnimation.item.start({x: root.width + userInput.width, y: userInput.y}, {x: userInput.x, y: userInput.y})
        bottomAnimation.item.start({x: controlAction.x, y: controlAction.y + controlAction.height}, {x: controlAction.x, y: controlAction.y})
    }

    function hide(): void {
        root.state = LoginAnimation.Hide
        leftAnimation.item.start({x: quickAction.x, y: quickAction.y}, {x: root.x - quickAction.width, y: quickAction.y})
        logoAnimation.item.start({x: logo.x, y: logo.y}, {x: root.x - logo.width, y: logo.y})
        rightAnimation.item.start({x: userInput.x, y: userInput.y}, {x: root.width + userInput.width, y: userInput.y})
        bottomAnimation.item.start({x: controlAction.x, y: controlAction.y}, {x: controlAction.x, y: controlAction.y + controlAction.height})
    }

    Component.onCompleted: {
        if (GreeterProxy.isLocked) {
            Qt.callLater(show)
        }
    }

    Loader {
        id: leftAnimation
        anchors.fill: parent
        sourceComponent: LoginAnimation {
            // qmllint disable unqualified: qmllint directive — quickAction and root are outer scope
            anchors.fill: parent
            target: quickAction
            state: root.state
            // qmllint enable unqualified
        }
    }

    Loader {
        id: logoAnimation
        anchors.fill: parent
        sourceComponent: LoginAnimation {
            // qmllint disable unqualified: qmllint directive — logo and root are outer scope
            anchors.fill: parent
            target: logo
            state: root.state
            // qmllint enable unqualified
        }
    }

    Loader {
        id: rightAnimation
        anchors.fill: parent
        sourceComponent: LoginAnimation {
            // qmllint disable unqualified: qmllint directive — userInput and root are outer scope
            state: root.state
            target: userInput
            anchors.fill: parent
            onStopped: {
                root.animationPlayFinished()
            }
            // qmllint enable unqualified
        }
    }

    Loader {
        id: bottomAnimation
        anchors.fill: parent
        sourceComponent: LoginAnimation {
            // qmllint disable unqualified: qmllint directive — controlAction and root are outer scope
            state: root.state
            target: controlAction
            anchors.fill: parent
            // qmllint enable unqualified
        }
    }

    Item {
        id: leftComp
        z: -1
        width: parent.width * 0.4
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            leftMargin: 30
            topMargin: 30
            bottomMargin: 30
        }
    }

    Item {
        id: rightComp
        z: -1
        anchors {
            left: leftComp.right
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            leftMargin: 30
            rightMargin: 30
            topMargin: 30
            bottomMargin: 30
        }
    }

    QuickAction {
        id: quickAction
        visible: !root.powerVisible
        anchors {
            verticalCenter : parent.verticalCenter
            right: leftComp.right
            rightMargin: leftComp.width / 9
        }
    }

    Row {
        id: logo
        anchors {
            bottom: leftComp.bottom
            left: leftComp.left
        }

        LogoProvider {
            id: logoProvider
        }

        Image {
            id: logoPic
            source: logoProvider.logo
            height: 32
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: logoProvider.version
            font.weight: Font.Normal
            font.pixelSize: 14
            color: Qt.rgba(1, 1, 1, 153 / 255)
        }
    }

    UserInput {
        id: userInput
        visible: !root.powerVisible
        anchors {
            verticalCenter: parent.verticalCenter
            left: rightComp.left
            leftMargin: rightComp.width / 5
        }

        focus: true
    }

    ControlAction {
        id: controlAction
        anchors {
            bottom: rightComp.bottom
            right: rightComp.right
        }
        rootItem: root
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    Connections {
        target: controlAction
        function onOtherUserRequested() {
            userInput.startOtherUserMode()
        }
    }

    Connections {
        target: GreeterProxy
        function onLockChanged(isLocked: bool) {
            if (isLocked) {
                root.show()
            } else {
                root.hide()
            }
        }
    }

    onAnimationPlayFinished: {
        if (!GreeterProxy.isLocked) {
            root.visible = false
        }
    }

    function showUserView(): void {
        root.animationPlayFinished.connect(root.__showUserList)
    }

    function __showUserList(): void {
        controlAction.showUserList()
        root.animationPlayFinished.disconnect(root.__showUserList)
    }
}
