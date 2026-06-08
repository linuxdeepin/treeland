// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import org.deepin.dtk 1.0 as D
import org.deepin.dtk.style 1.0 as DS

Control {
    id: root

    property bool activated: true

    // Keep theme detection aligned with DTK palette groups.
    palette.window: activated ? D.DTK.palette.window : D.DTK.inactivePalette.window

    // Active state: use full-featured palettes with all states
    property D.Palette activeTitlebarBackground: D.Palette {
        normal {
            common: "#ffffff"
            crystal: "#ffffff"
        }
        normalDark {
            common: "#282828"
            crystal: "#282828"
        }
    }

    property D.Palette activeTitleText: D.Palette {
        normal {
            common: "#303030"
            crystal: "#303030"
        }
        normalDark {
            common: "#8c8c8c"
            crystal: "#8c8c8c"
        }
    }

    // Inactive state: simplified palettes (no interaction expected)
    property D.Palette inactiveTitlebarBackground: D.Palette {
        normal: "#FCFCFC"
        normalDark: "#262626"
    }

    property D.Palette inactiveTitleText: D.Palette {
        normal: "#969696"
        normalDark: "#656565"
    }

    property D.Palette inactiveButtonText: D.Palette {
        normal: "#969696"
        normalDark: "#656565"
    }

    readonly property D.Palette buttonText: activated ? DS.Style.button.text : inactiveButtonText
    readonly property color titlebarBackground: D.ColorSelector.backgroundColor
    readonly property color titleText: D.ColorSelector.textColor

    D.ColorSelector.backgroundColor: activated ? activeTitlebarBackground : inactiveTitlebarBackground
    D.ColorSelector.textColor: activated ? activeTitleText : inactiveTitleText
}
