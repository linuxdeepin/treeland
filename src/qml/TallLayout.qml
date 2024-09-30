import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item { // Tall 布局
    id: tallLayout

    // property list <XdgSurfaceItem> panes: [] // list
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
        } else if (panes.length === 1) {
            panes[0].width = root.width / 2
            panes[0].height = root.height
            panes[0].visible = true
            surfaceItem.x = panes[0].x + panes[0].width
            surfaceItem.y = panes[0].y
            surfaceItem.width = root.width / 2
            surfaceItem.height = panes[0].height
            surfaceItem.visible = true
            panes.push(surfaceItem)
        } else {
            // 有两个以上的窗口，在右边分屏垂直，按照垂直布局处理
            let scale = (panes.length - 1) / (panes.length)
            panes[0].visible = true
            panes[1].y = 0
            panes[1].height *= scale
            panes[1].visible = true
            for (let i = 2; i < panes.length; i++) {
                panes[i].x = panes[i-1].x
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].width = panes[i-1].width
                panes[i].height *= scale
                panes[i].visible = true
            }
            surfaceItem.x = panes[panes.length - 1].x
            surfaceItem.y = panes[panes.length - 1].y + panes[panes.length - 1].height
            surfaceItem.height = root.height - surfaceItem.y
            surfaceItem.width = panes[panes.length - 1].width
            surfaceItem.visible = true
            panes.push(surfaceItem)
        }
        workspaceManager.syncWsPanes("addPane")
    }


    function choosePane(id) {
        currentSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        if (id === 1) { // selected for selectPane1
            let index = panes.indexOf(currentSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem1 = panes[0]
            } else {
                selectSurfaceItem1 = panes[index+1]
            }
            console.log("select1:", panes.indexOf(selectSurfaceItem1))
            Helper.activatedSurface = selectSurfaceItem1.shellSurface
        } else if (id === 2) { // selectd for selectPane2
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

    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        // let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let activeSurfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let index = panes.indexOf(activeSurfaceItem)
        if (panes.length === 1) {
            console.log("only one pane, cannot resize pane.")
            return
        }
        if (index === 0) { // 第一个窗格 在左边
            // let delta = size
            if (direction === 1) {
                console.log("You cannot resize the first pane on left!")
                return
            } else if (direction === 2) {
                activeSurfaceItem.width += size
                for (let i = 1; i < panes.length; i++) {
                    panes[i].x += size
                    panes[i].width -= size
                }
            }
        } else { // 在右边的窗格
            if (direction === 3 || direction === 4) {
                let last = panes.length - 1
                if (index === 1) { // 在右边的第一个窗格
                    if (direction === 3) { // 移动上边线以调整大小
                        console.log("You cannot resize the first pane on up!")
                        return
                    } else if (direction === 4) { // 用下边线调整大小
                        let delta = size / (panes.length - index - 1)
                        panes[1].height += size
                        for (let i = 2; i < panes.length; i++) {
                            panes[i].y = panes[i-1].y + panes[i-1].height
                            panes[i].height -= delta
                        }
                    }
                } else if (index === last) { // 在右边的最后一个窗格
                    if (direction === 4) {
                        console.log("You cannot resize the last pane on down!")
                        return
                    } else if (direction === 3) { // 用上边线调整大小
                        let delta = size / (index - 1)
                        panes[last].height -= size
                        panes[last].y += size
                        for (let i = last - 1; i >= 1; i--) {
                            panes[i].height += delta
                            panes[i].y = panes[i+1].y - panes[i].height
                        }
                        panes[1].y = 0
                        // console.log(panes[last].y, panes[last].height, panes[last].y + panes[last].height)
                    }
                } else if (index > 1 && index < last) { // 在右边的中间窗格
                    if (direction === 3) { // 用上边线调整大小
                        let delta = size / (index - 1)
                        panes[1].y = 0
                        panes[1].height += delta
                        for (let i = 2; i < index; i++) {
                            panes[i].height += delta
                            panes[i].y = panes[i-1].y + panes[i-1].height
                        }
                        activeSurfaceItem.y += size
                        activeSurfaceItem.height -= size
                    } else if (direction === 4) { // 用下边线调整大小
                        let delta = size / (panes.length - index - 1)
                        panes[last].height -= delta
                        panes[last].y += delta
                        for (let i = last - 1; i > index; i--) {
                            panes[i].height -= delta
                            panes[i].y = panes[i+1].y - panes[i].height
                        }
                        activeSurfaceItem.height += size
                    }
                }
            } else if (direction === 1) {
                panes[0].width += size
                for (let i = 1; i < panes.length; i++) {
                    panes[i].x += size
                    panes[i].width -= size
                }
            } else if (direction === 2) {
                console.log("You cannot resize the pane on right!")
                return
            }
        }
        workspaceManager.syncWsPanes("resizePane")
    }

    function removePane(flag) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        if (root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item === null) {
            console.log("You should choose a pane firstly.")
            return
        }

        let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        let index = panes.indexOf(surfaceItem)

        if (panes.length === 1) {
            panes.splice(index, 1)
            if (flag === 1) surfaceItem.shellSurface.closeSurface()
            else if (flag === 0) surfaceItem.visible = false
        } else if (panes.length === 2) { // 如果只有两个 直接扬成最大的
            panes.splice(index, 1)
            panes[0].x = 0
            panes[0].y = 0
            panes[0].width = root.width
            panes[0].height = root.height
            if (flag === 1) surfaceItem.shellSurface.closeSurface()
            else if (flag === 0) surfaceItem.visible = false
        } else if (index === 0) { // 两个以上窗口，删除左边的pane
            let scale = (panes.length - 1) / (panes.length - 2)
            panes[2].x = panes[1].x
            panes[2].y = panes[1].y
            panes[2].width = panes[1].width
            panes[2].height *= scale
            panes[1].x = panes[0].x // panes[0].x === 0
            panes[1].y = panes[0].y
            panes[1].width = panes[0].width
            panes[1].height = panes[0].height
            for (let i = 3; i < panes.length; ++i) {
                panes[i].height *= scale
                panes[i].y = panes[i-1].y + panes[i-1].height
            }
            panes.splice(index, 1)
            if (flag === 1) surfaceItem.shellSurface.closeSurface()
            else if (flag === 0) surfaceItem.visible = false
        } else { // 两个以上 pane，删除右边的 pane
            let scale = (panes.length - 1) / (panes.length - 2)
            panes.splice(index, 1)
            panes[1].y = 0
            panes[1].height *= scale
            for (let i = 2; i < panes.length; i++) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
            }
            if (flag === 1) surfaceItem.shellSurface.closeSurface()
            else if (flag === 0) surfaceItem.visible = false
        }
        workspaceManager.syncWsPanes("removePane")
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
            }
            if (panes[index2] === root.panes[i] && root.paneByWs[i] === currentWsId) {
                tempIndex2 = i
            }
        }

        if (index1 === 0 || index2 === 0) {
            let tempXPos = activeSurfaceItem.x
            let tempYPos = activeSurfaceItem.y
            let tempWidth = activeSurfaceItem.width
            let tempHeight = activeSurfaceItem.height
            activeSurfaceItem.x = targetSurfaceItem.x
            activeSurfaceItem.y = targetSurfaceItem.y
            activeSurfaceItem.width = targetSurfaceItem.width
            activeSurfaceItem.height = targetSurfaceItem.height
            targetSurfaceItem.x = tempXPos
            targetSurfaceItem.y = tempYPos
            targetSurfaceItem.width = tempWidth
            targetSurfaceItem.height = tempHeight
            // [panes[index1], panes[index2]] = [panes[index2], panes[index1]]
        } else {
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
    }

    function reLayout(surfaceItem) {
        let panes = workspaceManager.wsPanesById.get(currentWsId)
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 1) {
            panes.splice(index, 1)
        } else if (panes.length === 2) { // 如果只有两个 直接扬成最大的
            panes.splice(index, 1)
            panes[0].x = 0
            panes[0].y = 0
            panes[0].width = root.width
            panes[0].height = root.height
        } else if (index === 0) { // 两个以上窗口，删除左边的pane
            let scale = (panes.length - 1) / (panes.length - 2)
            panes[2].x = panes[1].x
            panes[2].y = panes[1].y
            panes[2].width = panes[1].width
            panes[2].height *= scale
            panes[1].x = panes[0].x // panes[0].x === 0
            panes[1].y = panes[0].y
            panes[1].width = panes[0].width
            panes[1].height = panes[0].height
            for (let i = 3; i < panes.length; ++i) {
                panes[i].height *= scale
                panes[i].y = panes[i-1].y + panes[i-1].height
            }
            panes.splice(index, 1)
            // if (flag === 1) surfaceItem.shellSurface.close()
            // else if (flag === 0) surfaceItem.visible = false
        } else { // 两个以上 pane，删除右边的 pane
            let scale = (panes.length - 1) / (panes.length - 2)

            panes.splice(index, 1)
            panes[1].y = 0
            panes[1].height *= scale
            for (let i = 2; i < panes.length; i++) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
            }
            // if (flag === 1) surfaceItem.shellSurface.close()
            // else if (flag === 0) surfaceItem.visible = false
        }

        // for (let i = 0; i < paneByWs.length; ++i) {
        //     if (root.panes[i] === surfaceItem) {
        //         paneByWs.splice(i, 1)
        //         root.panes.splice(i, 1)
        //         break
        //     }
        // }
        workspaceManager.syncWsPanes("reLayout")

    }
}
