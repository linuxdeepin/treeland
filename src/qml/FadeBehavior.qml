// Copyright (C) 2024 pengwenhao <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQml

Behavior {
    id: root

    property QtObject fadeTarget: targetProperty.object
    property string fadeProperty: "opacity"
    property int fadeDuration: 1000
    property real fadeValue: 0.5
    property string easingType: "Quad"

    SequentialAnimation {
        id: sequentialAnimation

        NumberAnimation {
            id: exitAnimation
            target: root.fadeTarget
            property: root.fadeProperty
            duration: root.fadeDuration
            to: root.fadeValue
            easing.type: root.easingType === "Linear" ? Easing.Linear : Easing["In"+root.easingType]
        }

        PauseAnimation {
            duration: 5
        }

        PropertyAction { }

        NumberAnimation {
            id: enterAnimation
            target: root.fadeTarget
            property: root.fadeProperty
            duration: root.fadeDuration
            to: target[property]
            easing.type:  root.easingType === "Linear" ? Easing.Linear : Easing["Out"+root.easingType]
        }
    }
}
