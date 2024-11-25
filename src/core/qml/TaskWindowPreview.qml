// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Treeland
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Loader {
    id: root

    signal clicked()
    property int loaderStatus: 0
    property SurfaceWrapper sourceSurface
    property alias previewComponent: sourceComponent

    transformOrigin: Item.Center
    property real preferredHeight: sourceSurface.height < (parent.height - 2 * vSpacing) ?
                                       sourceSurface.height : (parent.height - 2 * vSpacing)
    property real preferredWidth: sourceSurface.width < (parent.width - 2 * hSpacing) ?
                                      sourceSurface.width : (parent.width - 2 * hSpacing)
    property bool refHeight: preferredHeight *  sourceSurface.width / sourceSurface.height < (parent.width - 2 * hSpacing)
    readonly property real hSpacing: 20
    readonly property real vSpacing: 20

    height: refHeight ? preferredHeight : preferredWidth * sourceSurface.height / sourceSurface.width
    width: refHeight ? preferredHeight * sourceSurface.width / sourceSurface.height : preferredWidth

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
                ScaleAnimator {
                    target: root
                    from: 0.5
                    to: 1.0
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                OpacityAnimator {
                    target: root
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
                ScaleAnimator {
                    target: root
                    from: 1.0
                    to: 0.5
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                OpacityAnimator {
                    target: root
                    from: 1.0
                    to: 0.0
                    duration: 400
                    easing.type: Easing.OutExpo
                }
            }
        }
    ]

    Component {
        id: sourceComponent

        Item {
            anchors.fill: parent
            transformOrigin: root.transformOrigin

            SurfaceProxy {
                id: preview

                surface: sourceSurface
                anchors.centerIn: parent
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
    }
}
