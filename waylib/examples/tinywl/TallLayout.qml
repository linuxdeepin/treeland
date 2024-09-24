import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item { // Tall 布局
    id: tallLayout

    property list<XdgSurfaceItem> panes: [] // list
    property int spacing: 0 // 间距，暂时设置为 0
    property XdgSurfaceItem selectSurfaceItem1
    property XdgSurfaceItem selectSurfaceItem2

    Connections {
        target: Helper // sign
        onResizePane: resizePane(size, direction)
        onSwapPane: swapPane()
        onRemovePane: removePane()
        onChoosePane: choosePane(id)
        // onSwitchTall: switchLayout()
    }

    function addPane(surfaceItem) {
        if (panes.length === 0) {
            surfaceItem.x = 0
            surfaceItem.y = 0
            surfaceItem.width = root.width
            surfaceItem.height = root.height
            panes.push(surfaceItem)
        } else if (panes.length === 1) {
            panes[0].width = root.width / 2
            surfaceItem.x = panes[0].x + panes[0].width
            surfaceItem.width = root.width / 2
            panes.push(surfaceItem)
        } else {
            // 有两个以上的窗口，在右边分屏垂直，按照垂直布局处理
            let scale = panes.length / (panes.length + 1)
            panes[1].y = 0
            panes[1].height *= scale
            for (let i = 2; i < panes.length; i++) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
            }
            surfaceItem.y = panes[panes.length - 1].y + panes[panes.length - 1].height
            surfaceItem.height = root.height * (1.0 - scale)
            panes.push(surfaceItem)
        }
    }
    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        let activeSurfaceItem = Helper.activatedSurface
        let index = panes.indexOf(activeSurfaceItem)
        if (index === 0) { // 第一个窗格 在左边
            if (direction === 1) {
                console.log("You cannot resize the first pane on left!")
                return
            } else if (direction === 2) {
                activeSurfaceItem.width += size
                for (let i = 1; i < panes.length; i++) {
                    panes[i].x += size
                    panes[i].width -= size
                }
                return
            }
            return
        } else { // 在右边的窗格
            if (direction === 3 || direction === 4) {
                let last = panes.length - 1
                let delta = size / (panes.length - 1)
                if (index === 1) {
                    // 在右边的第一个窗格
                    if (direction === 3) {
                        // 移动上边线以调整大小
                        console.log("You cannot resize the first pane on up!")
                        return
                    } else if (direction === 4) {
                        panes[0].height += size
                        for (let i = 1; i < panes.length; i++) {
                            panes[i].y = panes[i-1].y + panes[i-1].height
                            panes[i].height -= delta
                        }
                    }
                } else if (index === last) {
                    // 在右边的最后一个窗格
                    if (direction === 4) {
                        console.log("You cannot resize the last pane on down!")
                        return
                    } else if (direction === 3) {
                        panes[last].height -= size
                        panes[last].y += size
                        for (let i = last - 1; i >= 0; i--) {
                            panes[i].height += delta
                            panes[i].y = panes[i+1].y - panes[i].height
                        }
                    }
                } else if (index > 1 && index < last) {
                    // 在右边的中间窗格
                    if (direction === 3) {
                        // 用上边线调整大小
                        panes[1].height += delta
                        panes[1].y = 0
                        for (let i = 2; i < index; i++) {
                            panes[i].height += delta
                            panes[i].y = panes[i-1].y + panes[i-1].height
                        }
                        activeSurfaceItem.height -= size
                        activeSurfaceItem.y += size
                    } else if (direction === 4) {
                        // 用下边线调整大小
                        panes[last].height -= delta
                        panes[last].y += delta
                        for (let i = last - 1; i > index; i--) {
                            panes[i].height -= delta
                            panes[i].y = panes[i+1].y - panes[i].height
                        }
                        activeSurfaceItem.height += size
                    }
                }
            } else if (direction === 3) {
                panes[0].width += size
                for (let i = 1; i < panes.length; i++) {
                    panes[i].x += size
                    panes[i].width -= size
                }
            } else if (direction === 4) {
                console.log("You cannot resize the pane on right!")
                return
            }
        }
    }

    function removePane() {
        let surfaceItem = selectedPane1
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 2) { // 如果只有两个 直接扬成最大的
            panes[1].x = 0
            panes[1].y = 0
            panes[1].width = root.width
            panes[1].height = root.height
            panes.splice(index, 1)
            surfaceItem.shellSurface.closeSurface()
        } else if (index === 0) { // 两个以上窗口，删除左边的pane
            let scale = (panes.length - 1) / (panes.length - 2)
            // panes[2].x = pane[1].x
            panes[2].y = pane[1].y
            // panes[2].width = pane[1].width
            panes[2].height = pane[1].height * scale
            panes[1].x = panes[0].x // panes[0].x === 0
            // panes[1].y = panes[0].y
            panes[1].width = panes[0].width
            panes[1].height = panes[0].height
            for (let i = 3; i < panes.length; ++i) {
                panes[i].height *= scale
                panes[i].y = panes[i-1].y + panes[i-1].height
            }
            panes.splice(index, 1)
            surfaceItem.shellSurface.closeSurface()
        } else { // 两个以上 pane，删除右边的 pane
            let scale = (panes.length - 1) / (panes.length - 2)
            panes.splice(index, 1)
            panes[1].y = 0
            panes[1].height *= scale
            for (let i = 2; i < panes.length; i++) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
            }
            surfaceItem.shellSurface.closeSurface()
        }
    }

    function swapPane() {
        let activatedSurface = selectSurfaceItem1
        let targetSurfaceItem = selectSurfaceItem2
        let index1 = panes.indexOf(activeSurfaceItem)
        let index2 = panes.indexOf(targetSurfaceItem)
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
            let tempYPos = activeSurfaceItem.y
            activeSurfaceItem.y = targetSurfaceItem.y
            let tempHeight = activeSurfaceItem.height
            activeSurfaceItem.height = targetSurfaceItem.height
            for (let i = index1 + 1; i <= index2 - 1; i++) {
                panes[i].y -= delta
            }
            targetSurfaceItem.y = tempYPos
            targetSurfaceItem.height = tempHeight
            // [panes[index1], panes[index2]] = [panes[index2], panes[index1]]
        }
    }
}
