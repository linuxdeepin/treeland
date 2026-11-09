// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

import Treeland
import org.deepin.dtk 1.0 as D

// The round close button the preview uses, both on a slot and in the title row.
D.ToolButton {
    id: closeButton

    property int maximumSize: PopoverStyle.closeButtonMaxSize
    // Rounded down to an even number so the circle keeps a whole-pixel centre.
    readonly property int diameter: 2 * Math.floor(maximumSize / 2)

    implicitWidth: diameter
    implicitHeight: diameter
    padding: 0
    icon.name: "close"
    icon.width: Math.round(diameter * 0.6)
    icon.height: Math.round(diameter * 0.6)
    // DTK paints the background as a rounded BoxPanel; half the size is a circle.
    Binding {
        target: closeButton.background
        property: "radius"
        value: closeButton.diameter / 2
        when: closeButton.background
    }
}
