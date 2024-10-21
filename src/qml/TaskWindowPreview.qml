// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Item {
    id: root

    signal clicked()
    property int loaderStatus: 0
    property alias sourceSueface : preview.surface

    onLoaderStatusChanged: {
        if (loaderStatus === -1) {
            enterAnimation.stop()
            exitAnimation.stop()
        }
    }
    states: [
        State {
            name: 'none'
            when: root.loaderStatus === 0
        },
        State {
            name: 'loaded'
            when: root.loaderStatus === 1
        }
    ]
    transitions: [
        Transition {
            from: "none"
            to: "loaded"

            ParallelAnimation {
                id: enterAnimation
                NumberAnimation {
                    target: root
                    property: "scale"
                    from: 0.5
                    to: 1.0
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                NumberAnimation {
                    target: root
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: 100
                    easing.type: Easing.OutExpo
                }
            }
        },
        Transition {
            from: "loaded"
            to: "none"

            ParallelAnimation {
                id: exitAnimation
                NumberAnimation {
                    target: root
                    property: "scale"
                    from: 1.0
                    to: 0.5
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                NumberAnimation {
                    target: root
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: 400
                    easing.type: Easing.OutExpo
                }
            }
        }
    ]

    SurfaceProxy {
        id: preview

        transformOrigin: root.transformOrigin
        maxSize: Qt.size(root.width, root.height)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: function(mouse) {
            root.clicked()
        }
    }

    TapHandler {
        acceptedButtons: Qt.NoButton
        acceptedDevices: PointerDevice.TouchScreen
        onDoubleTapped: function(eventPoint, button) {
            root.clicked()
        }
    }
}
