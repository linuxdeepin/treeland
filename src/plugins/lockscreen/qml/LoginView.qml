// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import Treeland
import LockScreen

// Global login UI container. Unlike Greeter (created once per output), this
// component is a single instance owned by the LockScreen container and
// repositioned to follow the output the cursor is on. It is created when the
// lock screen becomes visible and destroyed when it hides, so the login UI
// does not outlive the lock session.
FocusScope {
    id: root

    signal animationPlayed
    signal animationPlayFinished

    LockView {
        id: lockView
        anchors.fill: parent
        onAnimationPlayFinished: function () {
            if (lockView.state === LoginAnimation.Hide) {
                root.animationPlayFinished()
            }
        }
    }

    Loader {
        id: shutdownViewLoader
        anchors.fill: parent
        active: GreeterProxy.showShutdownView
        sourceComponent: shutdownViewComponent
    }

    Component {
        id: shutdownViewComponent
        ShutdownView {
            id: shutdownView
            anchors.fill: parent
            onSwitchUser: {
                root.switchUser()
            }
        }
    }

    /*****************************/
    /* Functions and Connections */
    /*****************************/

    function switchUser() {
        GreeterProxy.lock()
        lockView.showUserView()
    }

    Connections {
        target: GreeterProxy

        function onLockChanged(isLocked) {
            if (!isLocked)
                root.animationPlayed()
        }

        function onShowShutdownViewChanged(show) {
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
