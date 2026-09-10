// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Rectangle {
    id: root
    visible: false
    color: Qt.alpha(Helper.config.activeColor, 0.25)

    // The dragged surface's geometry (starting point) and the target tile
    // geometry, set from RootSurfaceContainer::updateEdgeTilePreview().
    property rect sourceGeometry: Qt.rect(0, 0, 0, 0)
    property rect targetGeometry: Qt.rect(0, 0, 0, 0)

    // Animation duration mirrors KWin's outline effect (Kirigami longDuration).
    property int animationDuration: 250
    // Animations stay disabled until the first show, so the initial transition
    // from the dragged surface to the target tile is animated (like KWin's
    // outline growing from the window being moved).
    property bool animationEnabled: false

    function applyGeometry(geometry) {
        x = geometry.x
        y = geometry.y
        width = geometry.width
        height = geometry.height
    }

    onVisibleChanged: {
        if (visible) {
            // Jump to the dragged surface first, then animate to the target.
            animationEnabled = false
            applyGeometry(sourceGeometry)
            animationEnabled = true
            applyGeometry(targetGeometry)
        } else {
            animationEnabled = false
        }
    }

    onTargetGeometryChanged: {
        if (visible)
            applyGeometry(targetGeometry)
    }

    Behavior on x {
        enabled: root.animationEnabled
        NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic }
    }
    Behavior on y {
        enabled: root.animationEnabled
        NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic }
    }
    Behavior on width {
        enabled: root.animationEnabled
        NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic }
    }
    Behavior on height {
        enabled: root.animationEnabled
        NumberAnimation { duration: root.animationDuration; easing.type: Easing.InOutCubic }
    }
}
