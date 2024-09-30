import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item { // 水平布局
    id: horizontalLayout

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
            panes[0].x = 0
            // panes[0].y = 0
            panes[0].width *= scale
            panes[0].visible = true
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].y = panes[0].y
                panes[i].width *= scale
                panes[i].height = panes[0].height
                panes[i].visible = true
            }
            surfaceItem.x = panes[panes.length - 1].x + panes[panes.length - 1].width
            surfaceItem.y = panes[0].y
            surfaceItem.width = root.width * (1.0 - scale)
            surfaceItem.height = panes[0].height
            surfaceItem.visible = true
            panes.push(surfaceItem)
        }
        workspaceManager.syncWsPanes("addPane")
    }

    // TODO: 选择 UI 不太彳亍
    function choosePane(id) {
        currentSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        if (id === 1) {
            let index = panes.indexOf(currentSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem1 = panes[0]
            } else {
                selectSurfaceItem1 = panes[index+1]
            }
            console.log("select1:", panes.indexOf(selectSurfaceItem1))
            Helper.activatedSurface = selectSurfaceItem1.shellSurface
        } else if (id === 2) {
            let index = panes.indexOf(currentSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem2 = panes[0]
            } else {
                selectSurfaceItem2 = panes[index+1]
            }
            console.log("select2:", panes.indexOf(selectSurfaceItem2))
            Helper.activatedSurface = selectSurfaceItem2.shellSurface
        }
    }

    function removePane(removeSurfaceIf) {
        let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 1) {
            panes.splice(index, 1)
            // surfaceItem.height = 0 // 移除 pane 时不需要修改 height 和 y
            if (removeSurfaceIf === 1) surfaceItem.shellSurface.closeSurface()
            else if(removeSurfaceIf === 0) surfaceItem.visible = false
        } else {
            let scale = panes.length / (panes.length - 1) // 放大系数
            panes.splice(index, 1)
            panes[0].x = 0
            panes[0].width *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].y = panes[0].y
                panes[i].width *= scale
                panes[i].height = panes[0].height
            }
            if (removeSurfaceIf === 1) surfaceItem.shellSurface.closeSurface()
            else if(removeSurfaceIf === 0) surfaceItem.visible = false
        }
        workspaceManager.syncWsPanes("removePane")
    }
    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        let activeSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(activeSurfaceItem)

        if (panes.length === 1) {
            console.log("only one pane, cannot resize pane.")
            return
        }
        if (index === 0) {
            let delta = size / (panes.length - 1)
            if (direction === 1) {
                console.log("You cannot left anymore!")
                return
            } else if (direction === 2) {
                activeSurfaceItem.width += size
                for (let i = 1; i < panes.length; ++i) {
                    panes[i].x = panes[i-1].x + panes[i-1].width
                    panes[i].width -= delta
                }
            }
        } else {
            let delta = size / index
            let last = panes.length - 1

            if (index === last) {
                if (direction === 1) {
                    panes[last].width -= size
                    panes[last].x += size
                    for (let i = last - 1; i >= 0; --i) {
                        panes[i].width += delta
                        panes[i].x = panes[i+1].x - panes[i].width
                    }
                    panes[0].x = 0
                } else if (direction === 2) {
                    console.log("You cannot left anymore!")
                    return
                }
            } else if (direction === 1) {
                // 用左边线改变宽度
                panes[0].x = 0
                panes[0].width += delta
                for (let i = 1; i < index; ++i) {
                    panes[i].x = panes[i-1].x + panes[i-1].width
                    panes[i].width += delta
                }
                activeSurfaceItem.x += size
                activeSurfaceItem.width -= size
            } else if (direction === 2) {
                panes[last].x += delta
                panes[last].width -= delta
                for (let i = last - 1; i > index; i--) {
                    panes[i].width -= delta
                    panes[i].x = panes[i+1].x - panes[i].width
                }
                activeSurfaceItem.x = activeSurfaceItem.x // x 不变
                activeSurfaceItem.width += size
            }
        }
        workspaceManager.syncWsPanes("resizePane")
    }

    function swapPane() {
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
            }
            if (panes[index2] === root.panes[i] && root.paneByWs[i] === currentWsId) {
                tempIndex2 = i
            }
        }

        let delta = activeSurfaceItem.width - targetSurfaceItem.width

        if (index1 < index2) {
            for (let i = index1 + 1; i <= index2 - 1; ++i) {
                panes[i].x -= delta
            }
            activeSurfaceItem.x = panes[index2 - 1].x + panes[index2 - 1].width
            targetSurfaceItem.x = panes[index1 + 1].x - panes[index2].width
        } else if (index1 > index2) {
            for (let i = index2 + 1; i <= index1 - 1; ++i) {
                panes[i].x += delta
            }
            activeSurfaceItem.x = panes[index2 + 1].x - panes[index1].width
            targetSurfaceItem.x = panes[index1 - 1].x + panes[index1 - 1].width
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
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 1) {
            panes.splice(index, 1)
            // 移除 pane 时不需要修改 height 和 y
        } else {
            let scale = panes.length / (panes.length - 1) // 放大系数
            panes.splice(index, 1)
            panes[0].x = 0
            panes[0].width *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].y = panes[0].y
                panes[i].width *= scale
                panes[i].height = panes[0].height
            }
        }
        workspaceManager.syncWsPanes("reLayout")
    }

}
