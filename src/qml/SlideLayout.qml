import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item {
    id: slideLayout

    property int spacing: 0 // 间距，暂时设置为 0
    function addPane(surfaceItem) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        surfaceItem.x = 0
        surfaceItem.y = 0
        surfaceItem.width = root.width
        surfaceItem.height = root.height
        surfaceItem.visible = true
        panes.push(surfaceItem)
        workspaceManager.syncWsPanes("addPane")
    }

    function swapPane() {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let currentSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        if (panes.length === 1) {
            console.log("You have only one pane.")
            return
        }
        let index = panes.indexOf(currentSurfaceItem)
        index += 1
        if (index === panes.length) {
            index = 0
        }
        for (let i = 0; i < panes.length; ++i) {
            if (i === index) panes[i].visible = true
            else panes[i].visible = false
        }
        Helper.activatedSurface = panes[index].shellSurface
        workspaceManager.syncWsPanes("swapPane")
    }

    function removePane(flag) {
        // bug: 如果 slideLayout 下只剩一个窗口时，删除会报错 116：ReferenceError: xwaylandComponent is not defined
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let index = panes.indexOf(surfaceItem)
        panes.splice(index, 1)
        if (flag) surfaceItem.shellSurface.closeSurface()
        else if(!flag) surfaceItem.visible = false
        if (panes.length) {
            let vis = -1
            for (let i = 0; i < panes.length; ++i) {
                if (panes[i].visible === true) { vis = 1; break}
            }
            if (vis === -1) panes[0].visible = true
        }
        workspaceManager.syncWsPanes("removePane")
    }

    function reLayout(surfaceItem) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(surfaceItem)
        panes.splice(index, 1)
        workspaceManager.syncWsPanes("reLayout")
    }
}
