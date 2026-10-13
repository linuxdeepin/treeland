// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import Waylib.Server
import Treeland
import QtQuick.Controls

Menu {
    id: menu
    modal: true

    property SurfaceWrapper surface: null
    readonly property bool canToggleMaximize: surface
        ? surface.surfaceState === SurfaceWrapper.State.Maximized || surface.isMaximizable
        : false

    onActiveFocusChanged: {
        if (!activeFocus)
            close()
    }

    onClosed: {
        surface = null
    }

    function showWindowMenu(surface, pos) {
        menu.surface = surface
        menu.parent = surface
        menu.popup(pos)
    }

    MenuItem {
        text: qsTr("Minimize")
        onTriggered: surface.minimize()
    }

    MenuItem {
        text: surface?.surfaceState === SurfaceWrapper.State.Maximized ? qsTr("Unmaximize") : qsTr("Maximize")
        enabled: menu.canToggleMaximize
        onTriggered: surface.toggleMaximized()
    }

    MenuItem {
        text: qsTr("Move")
        onTriggered: surface.moveRequested()
    }

    MenuItem {
        text: qsTr("Resize")
        onTriggered: WaylandServer.fakePressSurfaceBottomRightToReszie(surface)
    }

    MenuItem {
        checked: surface ? surface.alwaysOnTop : false
        text: qsTr("Always on Top")
        onTriggered: surface.alwaysOnTop = !surface.alwaysOnTop;
    }

    MenuItem {
        checked: surface ? surface.showOnAllWorkspace : false
        text: qsTr("Always on Visible Workspace")
        onTriggered: {
            if (surface.showOnAllWorkspace) {
                // Move to current workspace
                WaylandServer.workspace.moveSurfaceTo(surface, WaylandServer.workspace.current.id)
            } else {
                // Move to workspace 0, which is always visible
                WaylandServer.workspace.moveSurfaceTo(surface, -2)
            }
        }
    }

    MenuItem {
        property int leftWorkspaceId: surface ? WaylandServer.workspace.getLeftWorkspaceId(surface.workspaceId) : -1
        text: qsTr("Move to Left Work Space")
        enabled: leftWorkspaceId >= 0
        onTriggered: {
            WaylandServer.workspace.moveSurfaceTo(surface, leftWorkspaceId)
            WaylandServer.activateSurface(WaylandServer.workspace.current.latestActiveSurface())
        }
    }

    MenuItem {
        property int rightWorkspaceId: surface ? WaylandServer.workspace.getRightWorkspaceId(surface.workspaceId) : -1
        text: qsTr("Move to Right Work Space")
        enabled: rightWorkspaceId >= 0
        onTriggered: {
            WaylandServer.workspace.moveSurfaceTo(surface, rightWorkspaceId)
            WaylandServer.activateSurface(WaylandServer.workspace.current.latestActiveSurface())
        }
    }

    MenuItem {
        text: qsTr("Close")
        onTriggered: surface.shellSurface.close()
    }
}
