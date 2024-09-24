import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

Item {
    id: slideLayout
    property var panes: [] // list
    property int spacing: 0 // 间距，暂时设置为 0

    Connections {
        target: Helper // sign
        // onResize: resizePane(activeSurfaceItem, size, direction)
        onSwapPane: swapPane(activeSurfaceItem, targetSurfaceItem)
        onRemovePane: removePane(surfaceItem)
        onSwitchVertical: switchLayout()
    }

    function addPane(surfaceItem) {
        panes.push(surfaceItem)
        surfaceItem.x = 0
        surfaceItem.y = 0
        surfaceItem.width = root.width
        surfaceItem.height = root.height
    }

    function removePane(surfaceItem) {
        let index = panes.indexOf(surfaceItem)
        panes.splice(index, 1)
        surfaceItem.shellSurface.close()
    }

    // function 切换焦点的函数？
}
