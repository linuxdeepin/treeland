// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQml

Loader {
    id: root

    property alias blackComponent: rectComponent
    readonly property int aniDuration: 400
    property int loaderStatus: 0 // 0 none ; 1 loaded
    property real itemOpacity: 0.0

    Component {
        id: rectComponent

        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: root.itemOpacity

            states: [
                State {
                    name: 'none'
                    when: loaderStatus === 0
                },
                State {
                    name: 'loaded'
                    when: loaderStatus === 1
                }
            ]
            transitions: [
                Transition {
                    from: "none"
                    to: "loaded"

                    OpacityAnimator {
                        target: root.item
                        from: 0.0
                        to: 0.5
                        duration: root.aniDuration
                        easing.type: Easing.OutExpo
                    }
                },
                Transition {
                    from: "loaded"
                    to: "none"

                    onRunningChanged: {
                        if (running)
                            return

                        root.sourceComponent = undefined
                    }

                    OpacityAnimator {
                        target: root.item
                        from: 0.5
                        to: 0.0
                        duration: root.aniDuration
                        easing.type: Easing.OutExpo
                    }
                }
            ]
        }
    }
}
