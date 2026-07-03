// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick


Rectangle {
    property ListView sourceView: ListView.view
    readonly property int aniDuration: 400

    visible: sourceView.currentItem
    x: sourceView.currentItem ? sourceView.currentItem.x + sourceView.borderMargin : 0
    y: sourceView.currentItem ? sourceView.currentItem.y + (sourceView.vMargin / 2 - 2 * sourceView.borderMargin) : 0
    height: sourceView.currentItem ? sourceView.currentItem.height - 2 * (sourceView.vMargin / 2 - 2 * sourceView.borderMargin) : 0
    width: sourceView.currentItem ? sourceView.currentItem.width - 2 * sourceView.vSpacing + 2 * sourceView.borderMargin : 0
    color: "transparent"
    radius: sourceView.radius
    border {
        width: sourceView.borderWidth
        color: "#0081FF"
    }

    Behavior on x {
        enabled: sourceView.enableDelegateAnimation
        NumberAnimation {
            duration: aniDuration
            easing.type: Easing.OutExpo
        }
    }

    Behavior on width {
        enabled: sourceView.enableDelegateAnimation
        NumberAnimation {
            duration: aniDuration
            easing.type: Easing.OutExpo
        }
    }
}
