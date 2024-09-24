import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item { // 水平布局
    id: horizontalLayout

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
        // onSwitchHorizontal: switchLayout()
    }

    function addPane(surfaceItem) {
        if (panes.length === 0) {
            surfaceItem.x = 0
            surfaceItem.y = 0
            surfaceItem.width = root.width
            surfaceItem.height = root.height
            panes.push(surfaceItem)
        } else {
            let scale = panes.length / (panes.length + 1)
            panes[0].x = 0
            panes[0].width *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].width *= scale
            }
            surfaceItem.x = panes[panes.length - 1].x + panes[panes.length - 1].width
            surfaceItem.width = root.width * (1.0 - scale)
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
            surfaceItem.shellSurface.close()
        } else {
            let scale = panes.length / (panes.length - 1) // 放大系数
            panes.splice(index, 1)
            panes[0].x = 0
            panes[0].width *= scale
            for (let i = 1; i < panes.length; i++) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].width *= scale
            }
            surfaceItem.shellSurface.close()
        }
    }
    // direction: 1=左 2=右 3=上 4=下
    function resizePane(size, direction) {
        let activeSurfaceItem = Helper.activatedSurface
        let index = panes.indexOf(activeSurfaceItem)
        let delta = size / index
        if (direction === 1) {
            // 用左边线改变宽度
            if (index === 0) {
                // 第一个窗格
                console.log("You cannot left more!")
                return
            }
            panes[0].x = 0
            panes[0].width += delta
            for (let i = 1; i < index; ++i) {
                panes[i].x = panes[i-1].x + panes[i-1].width
                panes[i].width += delta
            }
            activeSurfaceItem.x += size
            activeSurfaceItem.width -= size
        } else if (direction === 2) {
            let last = panes.length - 1
            if (index === last) {
                // 用右边线改变宽度
                console.log("You can right more!")
                return
            }
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

    function swapPane() {
        let activeSurfaceItem = selectSurfaceItem1
        let targetSurfaceItem = selectSurfaceItem2
        let index1 = panes.indexOf(activeSurfaceItem)
        let index2 = panes.indexOf(targetSurfaceItem)
        let delta = activeSurfaceItem.width - targetSurfaceItem.width
        // swap X
        let tempXPos = activeSurfaceItem.x
        activeSurfaceItem.x = targetSurfaceItem.x
        // swap width
        let tempWidth = activeSurfaceItem.width
        activeSurfaceItem.width = targetSurfaceItem.width
        for (let i = index1 + 1; i <= index2 - 1; i++) {
            // 中间的窗口只改变 xPos
            panes[i].x -= delta
        }
        targetSurfaceItem.x = tempXPos
        targetSurfaceItem.width = tempWidth
        // [panes[index1], panes[index2]] = [panes[index2], panes[index1]]
    }
}
