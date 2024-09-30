import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server


Item { // 垂直布局
    id: verticalLayout

    property int spacing: 0 // 间距，暂时设置为 0
    property Item selectSurfaceItem1: null
    property Item selectSurfaceItem2: null
    property Item currentSurfaceItem: null

    function addPane(surfaceItem) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        if (panes.length === 0) {
            surfaceItem.x = 0
            surfaceItem.y = 0
            surfaceItem.width = root.width
            surfaceItem.height = root.height
            surfaceItem.visible = true
            panes.push(surfaceItem)
        } else {
            let scale = panes.length / (panes.length + 1)
            panes[0].y = 0
            panes[0].height *= scale
            panes[0].visible = true
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[0].x
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
                panes[i].width = panes[0].width
                panes[i].visible = true
            }
            surfaceItem.x = panes[0].x
            surfaceItem.y = panes[panes.length - 1].y + panes[panes.length - 1].height
            surfaceItem.height = root.height * (1.0 - scale)
            surfaceItem.width = panes[0].width
            surfaceItem.visible = true
            panes.push(surfaceItem)
        }
        workspaceManager.syncWsPanes("addPane")
    }


    // TODO: 选择 UI 不太彳亍
    function choosePane(id) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        currentSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        if (id === 1) { // selected for selectPane1
            let index = panes.indexOf(currentSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem1 = panes[0]
            } else {
                selectSurfaceItem1 = panes[index+1]
            }
            Helper.activatedSurface = selectSurfaceItem1.shellSurface
        } else if (id === 2) { // selectd for selectPane2
            let index = panes.indexOf(currentSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem2 = panes[0]
            } else {
                selectSurfaceItem2 = panes[index+1]
            }
            Helper.activatedSurface = selectSurfaceItem2.shellSurface
        }
    }

    function removePane(removeSurfaceIf) {
        let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 1) {
            panes.splice(index, 1)
            // 移除 pane 时不需要修改 height 和 y
            if (removeSurfaceIf === 1) surfaceItem.shellSurface.closeSurface()
            else if (removeSurfaceIf === 0) surfaceItem.visible = false
        } else {
            let scale = panes.length / (panes.length - 1) // 放大系数
            panes.splice(index, 1)
            panes[0].y = 0
            panes[0].height *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[0].x
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
                panes[i].width = panes[0].width
            }
            if (removeSurfaceIf === 1) surfaceItem.shellSurface.closeSurface()
            else if (removeSurfaceIf === 0) surfaceItem.visible = false
        }
        workspaceManager.syncWsPanes("removePane")
    }

    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        // console.log("vertical layout resize pane")
        let activeSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(activeSurfaceItem)

        if (panes.length === 1) {
            console.log("only one pane, cannot resize pane.")
            return
        }

        if (index === 0) {
            let delta = size / (panes.length - 1)
            if (direction === 3) {
                console.log("You cannot up anymore!")
                return
            } else if (direction === 4) {
                activeSurfaceItem.height += size
                for (let i = 1; i < panes.length; i++) {
                    panes[i].y = panes[i-1].y + panes[i-1].height;
                    panes[i].height -= delta
                }
            }
        } else {
            let delta = size / index
            let last = panes.length - 1
            if (index === last) {
                if (direction === 3) {
                    panes[last].height -= size
                    panes[last].y += size
                    for (let i = last - 1; i >= 0; --i) {
                        panes[i].height += delta
                        panes[i].y = panes[i+1].y - panes[i].height
                    }
                    panes[0].y = 0
                } else if (direction === 4) {
                    console.log("You cannot down anymore!")
                    return
                }
            } else if (direction === 3) {
                panes[0].y = 0
                panes[0].height += delta
                // 中间的窗格
                for (let i = 1; i < index; ++i) {
                    panes[i].y = panes[i-1].y + panes[i-1].height
                    panes[i].height += delta
                }
                // 当前窗格
                activeSurfaceItem.y += size
                activeSurfaceItem.height -= size
            } else if (direction === 4) {
                // 最后一个窗格
                panes[last].y += delta
                panes[last].height -= delta
                for (let i = last - 1; i > index; i--) {
                    panes[i].height -= delta
                    panes[i].y = panes[i+1].y - panes[i].height
                }
                activeSurfaceItem.y = activeSurfaceItem.y // y 不变
                activeSurfaceItem.height += size
            }
        }
        workspaceManager.syncWsPanes("resizePane")
    }

    function swapPane() {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        if (selectSurfaceItem1 === null || selectSurfaceItem2 === null) {
            console.log("You should choose two pane before swap")
            return
        }
        let activeSurfaceItem = selectSurfaceItem1
        let targetSurfaceItem = selectSurfaceItem2
        let index1 = panes.indexOf(activeSurfaceItem)
        let index2 = panes.indexOf(targetSurfaceItem)
        if (index1 === index2) {
            console.log("You should choose two different panes to swap.")
            return
        }
        let tempIndex1 = null
        let tempIndex2 = null
        for (let i = 0; i < root.panes.length; ++i) {
            if (panes[index1] === root.panes[i] && root.paneByWs[i] === currentWsId) {
                tempIndex1 = i
                // break
            }
            if (panes[index2] === root.panes[i] && root.paneByWs[i] === currentWsId) {
                tempIndex2 = i
                // break
            }
        }

        let delta = activeSurfaceItem.height - targetSurfaceItem.height
        if (index1 < index2) {
            for (let i = index1 + 1; i <= index2 - 1; i++) {
                // 中间的窗口只改变 yPos
                panes[i].y -= delta

            }
            activeSurfaceItem.y = panes[index2 - 1].y + panes[index2 - 1].height
            targetSurfaceItem.y = panes[index1 + 1].y - panes[index2].height
        } else if (index1 > index2) {
            for (let i = index2 + 1; i <= index1 - 1; ++i) {
                panes[i].y += delta
            }
            activeSurfaceItem.y = panes[index2 + 1].y - panes[index1].height
            targetSurfaceItem.y = panes[index1 - 1].y + panes[index1 - 1].height
        }

        let temp1 = root.panes[tempIndex1]
        root.panes[tempIndex1] = root.panes[tempIndex2]
        root.panes[tempIndex2] = temp1
        let temp2 = panes[index1];
        panes[index1] = panes[index2];
        panes[index2] = temp2;

        workspaceManager.syncWsPanes("swapPane")
    }

    function reLayout(surfaceItem) {
        if (deleteFlag === 1) {
            deleteFlag = -1
            return
        }
        let wsPanes = workspaceManager.wsPanesById.get(currentWsId)
        let scale = wsPanes.length / (wsPanes.length - 1)
        let index = wsPanes.indexOf(surfaceItem)
        wsPanes.splice(index, 1)
        wsPanes[0].y = 0
        wsPanes[0].height *= scale
        for (let i = 1; i < wsPanes.length; ++i) {
            wsPanes[i].y = wsPanes[i-1].y + wsPanes[i-1].height
            wsPanes[i].height *= scale
        }
        workspaceManager.syncWsPanes("reLayout")
    }
}
