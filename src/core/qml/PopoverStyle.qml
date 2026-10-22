// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton

import QtQuick
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

// Style for the popovers the compositor draws itself. Mainly referenced from DTK.
QtObject {
    id: style

    readonly property bool isDark: D.DTK.themeType === D.ApplicationHelper.DarkType
    readonly property int radius: DS.Style.control.radius

    readonly property color tint: isDark ? DS.Style.behindWindowBlur.darkColor
                                         : DS.Style.behindWindowBlur.lightColor
    readonly property color opaqueTint: isDark ? DS.Style.behindWindowBlur.darkNoBlurColor
                                               : DS.Style.behindWindowBlur.lightNoBlurColor

    function tintFor(blurred) {
        return blurred ? tint : opaqueTint
    }

    readonly property font textFont: D.DTK.fontManager.t8
    readonly property font titleFont: D.DTK.fontManager.t6


    readonly property color textColor: D.DTK.palette.brightText
    readonly property color titleColor: D.DTK.palette.windowText

    // dde-shell overrides DTK's toolTip padding horizontally (8 instead of 5),
    readonly property int tooltipHorizontalPadding: 8
    readonly property int tooltipVerticalPadding: DS.Style.toolTip.verticalPadding

    // A DTK tooltip is a fixed-height pill; keeping it an integer also keeps the
    // 1px border and the corner radius on whole pixels.
    readonly property int tooltipMinimumHeight: DS.Style.toolTip.height

    readonly property int shadowBlur: 20
    readonly property int shadowOffsetY: 6
    readonly property color shadowColor: DS.Style.arrowRectangleBlur.shadowColor

    readonly property color borderOutsideColor: DS.Style.arrowRectangleBlur.outBorderColor
    readonly property color borderInsideColor: DS.Style.arrowRectangleBlur.inBorderColor

    // Upper bound for the floating close button, so it can never grow a row it
    // is shown in. DTK's own floating button is this size.
    readonly property int closeButtonMaxSize: DS.Style.floatingButton.size

    // Placeholder behind a preview thumbnail. This is DPalette::ItemBackground,
    // which DTK does not export to QML in this version, so the two palette
    // values are kept here instead of being inlined in DockPreview.qml.
    readonly property color itemBackground: isDark ? Qt.rgba(1, 1, 1, 0.05)
                                                   : Qt.rgba(0, 0, 0, 0.03)
}
