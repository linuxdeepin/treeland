// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Effects
import Waylib.Server
import TreeLand
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Loader {
    id: root

    signal clicked()

    property int loaderStatus: 0 // 0 none ; 1 loaded
    property SurfaceItem sourceSueface
    property alias previewComponent: sourceComponent
    property real cornerRadius
    property int itemTransformOrigin: Item.Center
    property real preferredHeight: sourceSueface.height < (parent.height - 2 * vSpacing) ?
                                       sourceSueface.height : (parent.height - 2 * vSpacing)
    property real preferredWidth: sourceSueface.width < (parent.width - 2 * hSpacing) ?
                                      sourceSueface.width : (parent.width - 2 * hSpacing)
    property bool refHeight: preferredHeight *  sourceSueface.width / sourceSueface.height < (parent.width - 2 * hSpacing)
    property D.Palette outerShadowColor: DS.Style.highlightPanel.dropShadow
    readonly property real hSpacing: 20
    readonly property real vSpacing: 20

    height: refHeight ? preferredHeight : preferredWidth * sourceSueface.height / sourceSueface.width
    width: refHeight ? preferredHeight * sourceSueface.width / sourceSueface.height : preferredWidth

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
                    target: root.item
                    from: 0.5
                    to: 1.0
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                OpacityAnimator {
                    target: root.item
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
                ScaleAnimator {
                    target: root.item
                    from: 1.0
                    to: 0.5
                    duration: 400
                    easing.type: Easing.OutExpo
                }

                OpacityAnimator {
                    target: root.item
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
            transformOrigin: root.itemTransformOrigin

            D.BoxShadow {
                anchors.fill: parent
                shadowColor: root.D.ColorSelector.outerShadowColor
                shadowOffsetY: 4
                shadowBlur: 16
                cornerRadius: root.cornerRadius
                hollow: true
            }

            ShaderEffectSource {
                id: preview

                anchors.fill: parent
                sourceItem: sourceSueface
                visible: false
                transformOrigin: root.itemTransformOrigin
            }

            MultiEffect {
                enabled: root.cornerRadius > 0
                anchors.fill: preview
                source: preview
                maskEnabled: true
                maskSource: mask
            }

            Item {
                id: mask

                width: preview.width
                height: preview.height
                layer.enabled: true
                visible: false
                transformOrigin: root.itemTransformOrigin

                Rectangle {
                    width: preview.width
                    height: preview.height
                    radius: root.cornerRadius
                }
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
