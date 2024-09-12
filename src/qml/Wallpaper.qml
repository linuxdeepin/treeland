// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import TreeLand.Utils

WallpaperProxy {
    id: root
    clip: true
    property bool isAnimated: false

    Loader {
        id: imageLoader
        anchors.fill: parent
    }

    Component {
        id: animatedImage
        AnimatedImage {
            id: animatedBackground
            source: root.source

            fillMode: Image.PreserveAspectCrop
            anchors.fill: parent
            asynchronous: true
        }
    }

    Component {
        id: staticImage
        Image {
            id: staticBackground
            source: root.source

            fillMode: Image.PreserveAspectCrop
            anchors.fill: parent
            asynchronous: true

            FadeBehavior on source { }
        }
    }

    states: [
        State {
            name: "Normal"
            PropertyChanges {
                target: imageLoader.item
                scale: 1
            }
        },
        State {
            name: "Scale"
            PropertyChanges {
                target: imageLoader.item
                scale: 1.4
            }
        }
    ]

    transitions: [
        Transition {
            from: "*"
            to: "Normal"
            PropertyAnimation {
                property: "scale"
                duration: 1000
                easing.type: Easing.OutExpo
            }
        },
        Transition {
            from: "*"
            to: "Scale"
            PropertyAnimation {
                property: "scale"
                duration: 1000
                easing.type: Easing.OutExpo
            }
        }
    ]

    onSourceChanged: {
        if (isAnimated) {
            imageLoader.sourceComponent = animatedImage
        } else {
            imageLoader.sourceComponent = staticImage
        }
    }

    onTypeChanged: {
        background.state = type === Helper.Normal ? "Normal" : "Scale";
    }
}
