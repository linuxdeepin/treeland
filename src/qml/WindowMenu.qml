// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import Waylib.Server
import Treeland
import org.deepin.dtk 1.0 as D

D.Menu {
    id: menu

    property SurfaceWrapper surface: null

    function showWindowMenu(surface, pos) {
        menu.surface = surface
        menu.parent = surface
        menu.popup(pos)
    }

    D.MenuItem {
        text: qsTr("Minimize")
        onTriggered: surface.requestMinimize()
    }

    D.MenuItem {
        text: surface?.surfaceState === SurfaceWrapper.State.Maximized ? qsTr("Unmaximize") : qsTr("Maximize")
        onTriggered: surface.requestToggleMaximize()
    }

    D.MenuItem {
        text: qsTr("Move")
        onTriggered: surface.requestMove()
    }

    D.MenuItem {
        text: qsTr("Resize")
        onTriggered: Helper.fakePressSurfaceBottomRightToReszie(surface)
    }

    D.MenuItem {
        checked: surface ? surface.alwaysOnTop : false
        text: qsTr("Always on Top")
        onTriggered: surface.alwaysOnTop = !surface.alwaysOnTop;
    }

    D.MenuItem {
        checked: surface ? surface.showOnAllWorkspace : false
        text: qsTr("Always on Visible Workspace")
        onTriggered: {
            if (surface.showOnAllWorkspace) {
                // Move to current workspace
                Helper.workspace.moveSurfaceTo(surface, Helper.workspace.current.id)
            } else {
                // Move to workspace 0, which is always visible
                Helper.workspace.moveSurfaceTo(surface, -2)
            }
        }
    }

    D.MenuItem {
        property int leftWorkspaceId: surface ? Helper.workspace.getLeftWorkspaceId(surface.workspaceId) : -1
        text: qsTr("Move to Left Work Space")
        enabled: leftWorkspaceId >= 0
        onTriggered: Helper.workspace.moveSurfaceTo(surface, leftWorkspaceId)
    }

    D.MenuItem {
        property int rightWorkspaceId: surface ? Helper.workspace.getRightWorkspaceId(surface.workspaceId) : -1
        text: qsTr("Move to Right Work Space")
        enabled: rightWorkspaceId >= 0
        onTriggered: Helper.workspace.moveSurfaceTo(surface, rightWorkspaceId)
    }

    D.MenuItem {
        text: qsTr("Close")
        onTriggered: surface.shellSurface.close()
    }
}
