import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server


Item { // 垂直布局
    id: verticalLayout

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
        // onSwitchVertical: switchLayout()
    }

    function addPane(surfaceItem) {
        console.log("panes.length:", panes.length)
        if (panes.length === 0) {
            surfaceItem.x = 0
            surfaceItem.y = 0
            surfaceItem.width = root.width
            surfaceItem.height = root.height
            // panes.append(surfaceItem)
            panes.push(surfaceItem)
            // console.log("Here!!!!")
        } else {
            let scale = panes.length / (panes.length + 1)
            panes[0].y = 0
            panes[0].height *= scale

            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[0].x
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
                panes[i].width = panes[0].width
            }
            surfaceItem.x = panes[0].x
            surfaceItem.y = panes[panes.length - 1].y + panes[panes.length - 1].height
            surfaceItem.height = root.height * (1.0 - scale)
            surfaceItem.width = panes[0].width
            panes.push(surfaceItem)
        }
    }

    function choosePane(id) {
        if (id === 1) {
            let activeSurfaceItem = Helper.activatedSurface
            let index = panes.indexOf(activeSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem1 = panes[0]
            } else {
                selectSurfaceItem1 = panes[index+1]
            }
            Helper.activatedSurface = selectSurfaceItem1
        } else if (id === 2) {
            let activeSurfaceItem = Helper.activatedSurface
            let index = panes.indexOf(activeSurfaceItem)
            if (index === panes.length - 1) {
                selectSurfaceItem2 = panes[0]
            } else {
                selectSurfaceItem2 = panes[index+1]
            }
            Helper.activatedSurface = selectSurfaceItem2
        }
    }

    function removePane() {
        let surfaceItem = selectSurfaceItem1
        let index = panes.indexOf(surfaceItem)
        if (panes.length === 1) {
            panes.splice(index, 1)
            // surfaceItem.height = 0 // 移除 pane 时不需要修改 height 和 y
            surfaceItem.shellSurface.closeSurface()
        } else {
            let scale = panes.length / (panes.length - 1) // 放大系数
            panes.splice(index, 1)
            panes[0].y = 0
            panes[0].height *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height *= scale
            }
            surfaceItem.shellSurface.closeSurface()
        }
    }
    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        let activeSurfaceItem = Helper.activeSurfaceItem
        let index = panes.indexOf(activeSurfaceItem)
        let delta = size / index
        if (direction === 3) {
            // 用上边线改变高度
            if (index === 0) {
                // 第一个窗格
                console.log("You cannot up more!")
                return
            }
            // 第一个窗格
            panes[0].y = 0
            panes[0].height -= delta
            // 中间的窗格
            for (let i = 1; i < index; ++i) {
                panes[i].y = panes[i-1].y + panes[i-1].height
                panes[i].height -= delta
            }
            // 当前窗格
            activeSurfaceItem.y -= size
            activeSurfaceItem.height += size
        } else if (direction === 4) {
            // 用下边线改变高度
            let last = panes.length - 1
            if (index === last) {
                // 最后一个窗格
                console.log("You can down more!")
                return
            }
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

    function swapPane() {
        let activatedSurface = selectSurfaceItem1
        let targetSurfaceItem = selectSurfaceItem2
        let index1 = panes.indexOf(activeSurfaceItem)
        let index2 = panes.indexOf(targetSurfaceItem)
        let delta = activeSurfaceItem.height - targetSurfaceItem.height
        // swap y
        let tempYPos = activeSurfaceItem.y
        activeSurfaceItem.y = targetSurfaceItem.y
        // swap height
        let tempHeight = activeSurfaceItem.height
        activeSurfaceItem.height = targetSurfaceItem.height
        for (let i = index1 + 1; i <= index2 - 1; i++) {
            // 中间的窗口只改变 yPos
            panes[i].y += delta
        }
        targetSurfaceItem.y = tempYPos
        targetSurfaceItem.height = tempHeight
        // [panes[index1], panes[index2]] = [panes[index2], panes[index1]]
    }
}
