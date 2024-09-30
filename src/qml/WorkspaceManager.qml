import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

// 工作区的实现

Item {
    id: workspaceManager
    property var wsPanesById: new Map() // id : panes[...] 管理某个工作区的 panes
    property var wsLayoutById: new Map()

    function createWs() {
        console.log("create ws")
        if (currentWsId === -1) {
            currentWsId = 0
            wsPanesById.set(0, [])
            wsLayoutById.set(0, currentLayout)
        } else {
            let size = wsPanesById.size
            wsPanesById.set(size, [])
            wsLayoutById.set(size, currentLayout)
        }
    }

    function syncWsPanes(opClass) {
        // console.log("sync ws")
        // let cnt = 0
        if (opClass === "addPane") {
            console.log("sync addPane")
            // let cnt = 0
            panes.push(wsPanesById.get(currentWsId)[wsPanesById.get(currentWsId).length - 1])

            // console.log(paneByWs)
            // console.log(panes)
            // console.log(wsPanesById.get(currentWsId))
            // cnt = 0
        } else if (opClass === "removePane") {
            console.log("sync removePane")
            let cnt = 0
            let index = -1
            // console.log(paneByWs)
            // console.log(wsPanesById.get(currentWsId))
            for (let i = 0; i < paneByWs.length; ++i) {
                if (paneByWs[i] === currentWsId && wsPanesById.get(currentWsId).indexOf(panes[i]) === -1) { // 找到在 wsPanes 里被删除而panes里未被删除的窗口
                    // panes.splice(i, 1)
                    index = i
                    break // 一次只会删除一个 pane
                }
            }
            panes.splice(index, 1)
            paneByWs.splice(index, 1)
            for (let i = 0; i < paneByWs.length; ++i) {
                if (paneByWs[i] === currentWsId) { // 更新 panes 里该工作区 pane 的大小
                    panes[i].x = wsPanesById.get(currentWsId)[cnt].x
                    panes[i].y = wsPanesById.get(currentWsId)[cnt].y
                    panes[i].height = wsPanesById.get(currentWsId)[cnt].height
                    panes[i].width = wsPanesById.get(currentWsId)[cnt].width
                    ++cnt;
                }
            }
            // console.log(panes)
            // console.log(paneByWs)
        } else if (opClass === "resizePane" ) {
            console.log()
            let cnt = 0
            for (let i = 0; i < paneByWs.length; ++i) {
                if (paneByWs[i] === currentWsId) {
                    panes[i].x = wsPanesById.get(currentWsId)[cnt].x
                    panes[i].y = wsPanesById.get(currentWsId)[cnt].y
                    panes[i].height = wsPanesById.get(currentWsId)[cnt].height
                    panes[i].width = wsPanesById.get(currentWsId)[cnt].width
                    ++cnt
                }
            }
        } else if (opClass === "swapPane") {
            let cnt = 0
            for (let i = 0; i < paneByWs.length; ++i) {
                if (paneByWs[i] === currentWsId) {
                    panes[i].x = wsPanesById.get(currentWsId)[cnt].x
                    panes[i].y = wsPanesById.get(currentWsId)[cnt].y
                    panes[i].height = wsPanesById.get(currentWsId)[cnt].height
                    panes[i].width = wsPanesById.get(currentWsId)[cnt].width
                    ++cnt
                }
            }
            for (let i = 0; i < root.panes.length; ++i) {
                if (paneByWs[i] === currentWsId) {
                    root.panes[i].visible = wsPanesById.get(currentWsId)[index].visible
                }
            }
        }
        else if (opClass === "reLayout") {
            console.log("sync reLayout")
            let cnt = 0
            // console.log(panes)
            // console.log(wsPanesById.get(currentWsId))
            for (let i = 0; i < paneByWs.length; ++i) {
                if (panes[i] === root.toplevelVerticalSufaceItem) {
                    paneByWs.splice(i, 1)
                    panes.splice(i, 1)
                    break
                }
            }
            for (let i = 0; i < paneByWs.length; ++i) {
                if (paneByWs[i] === currentWsId) {
                    // console.log()
                    panes[i].x = wsPanesById.get(currentWsId)[cnt].x
                    panes[i].y = wsPanesById.get(currentWsId)[cnt].y
                    panes[i].height = wsPanesById.get(currentWsId)[cnt].height
                    panes[i].width = wsPanesById.get(currentWsId)[cnt].width
                    ++cnt
                }
            }
        }
    }

    function switchNextWs() {
        console.log("switch ws")
        let toId = currentWsId + 1
        let fromId = currentWsId
        if (toId === wsPanesById.size) {
            toId = 0
        }
        console.log(fromId, toId)
        let fromPanes = wsPanesById.get(fromId)
        for (let i = 0; i < fromPanes.length; ++i) {
            fromPanes[i].visible = false
        }
        let toPanes = wsPanesById.get(toId)
        for (let i = 0; i < toPanes.length; ++i) {
            toPanes[i].visible = true
        }
        currentWsId = toId
    }

    function destoryWs() {
        console.log("destory ws")
        if (wsPanesById.size === 1) {
            console.log("you have only one workspace so you cannot destory it.")
            return
        }
        let wsPanes = wsPanesById.get(currentWsId)
        let len = wsPanes.length
        let cnt = 0
        for (let i = 0; i < panes.length; ++i) {
            if (paneByWs[i] === currentWsId) {
                cnt += 1
            }
        }
        for (let i = 0; i < panes.length; ++i) {
            if (paneByWs[i] === currentWsId) {
                for (let j = 0; j < cnt; ++j) panes[i+j].shellSurface.closeSurface()
                panes.splice(i, len)
                paneByWs.splice(i, len)
                break
            }
        }
        for (let i = 0; i < len; ++i) {
            wsPanes.splice(0, 1)
        }
        deleteFlag = 1
        wsPanesById.delete(currentWsId)
        wsLayoutById.delete(currentWsId)
        --currentWsId
        if (wsPanesById.size === 0) {
            console.log("you have no workspace.")
            return
        } else {
            for (let i = 0; i < wsPanesById.get(currentWsId).length; ++i) {
                wsPanesById.get(currentWsId)[i].visible = true
            }
        }
    }

    function moveWs(currentWsId, toWsId) {
        // layoutOrder.move(fromid, toid, 1)
        let surfaceItem = root.getSurfaceItemFromWaylandSurface(Helper.activatedSurface).item
        // let currentPanes = layoutOrder.get(currentWsId).panes
        // let currentLayout = layoutOrder.get(currentWsId).layout
        // let toPanes = layoutOrder.get(toWsId).panes
        // let toLayout = layoutOrder.get(toWsId).layout
        // currentLayout.removePane(1)
        // toLayout.addPane()
    }
}
