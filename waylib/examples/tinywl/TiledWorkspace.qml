// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Waylib.Server

// import "./VerticalLayout.qml" as CustomComponents

// 工作区的实现
// QtObject {
//     id: workspaceManager
//     property var workspacesById: new Map()
//     property alias nWorkspaces: layoutOrder.count
//     property var __: QtObject {
//         id: privt
//         property int workspaceIdCnt: { workspaceIdCnt = layoutOrder.count - 1 }
//     }
//     function createWs() {
//         layoutOrder.push({ wsid: ++privt.workspaceIdCnt, layout: "tiled" })
//     }
//     function destroyWs(id) {
//         console.log(`workspace id=${id} destroyed!`)
//         layoutOrder.remove(id)
//     }
//     function moveWs(from, to) {
//         layoutOrder.move(from, to, 1)
//     }
//     function setLayout(id, layout) {
//         for (let i = 0; i < layoutOrder.count; i++) {
//             if (layoutOrder.get(i).wsid === id) {
//                 layoutOrder.setProperty(i, "layout", layout)
//                 break
//             }
//             onObjectRemoved: function (obj) {
//                 let index = verticalLayout.panes.indexOf(obj)
//                 verticalLayout.panes.splice(index, 1)
//                 obj.doDestroy()
//                 updateLayout()
//             }
//         }
//     }
//     property ListModel layoutOrder: ListModel {
//         id: layoutOrder
//         objectName: "layoutOrder"
//         ListElement {
//             wsid: 0
//             layout: "tiled"
//         }
//     }
//     property ListModel allSurfaces: ListModel {
//         id: allSurfaces
//         objectName: "allSurfaces"
//         function removeIf(cond) {
//             for (let i = 0; i < this.count; i++) {
//                 if ( cond(this.get(i))) {
//                     this.remove(i)
//                     return
//                 }
//             }
//         }
//     }
//     Component.onCompleted: {
//         createWs(), createWs()
//     }
// }



Item {
    id: root


// 4 种 layout --------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------

    function getSurfaceItemFromWaylandSurface(surface) {
        let finder = function(props) {
            if (!props.waylandSurface)
                return false
            // surface is WToplevelSurface or WSurfce
            if (props.waylandSurface === surface || props.waylandSurface.surface === surface)
                return true
        }

        let toplevel = Helper.xdgShellCreator.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                shell: toplevel,
                item: toplevel,
                type: "toplevel"
            }
        }

        let popup = Helper.xdgShellCreator.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface,
                type: "popup"
            }
        }

        let layer = Helper.layerShellCreator.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem,
                type: "layer"
            }
        }

        let xwayland = Helper.xwaylandCreator.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland,
                type: "xwayland"
            }
        }

        return null
    }


    VerticalLayout { // default layout
        id: defaultLayout
        anchors.fill: parent

        // verticalLayout

        // 创建 pane
        DynamicCreatorComponent {
            id: paneCreator
            creator: Helper.xdgShellCreator
            chooserRole: "type"
            chooserRoleValue: "toplevel"
            autoDestroy: false

            onObjectRemoved: function (obj) {
                obj.doDestroy()
            }

            // xdgSurface 是窗口本身
            XdgSurface {
                id: toplevelVerticalSurfaceItem     
                resizeMode: SurfaceItem.SizeToSurface

                Component.onCompleted: {
                    // console.log("VerticalLayout:", VerticalLayout)
                    // console.log("toplevelVerticalSurfaceItem:", toplevelVerticalSurfaceItem)    
                    // console.log(typeof toplevelVerticalSurfaceItem)
                    // console.log("break point 1")
                    // VerticalLayout.addPane(toplevelVerticalSurfaceItem)
                    // console.log("break point 2")

                    console.log("Component completed")
                    console.log("toplevelVerticalSurfaceItem:", toplevelVerticalSurfaceItem)
                    if (toplevelVerticalSurfaceItem) {
                        console.log("aaaaaaa")
                        console.log("VerticalLayout:", verticalLayout)
                        console.log("addpane:", typeof verticalLayout.addPane)
                        if (VerticalLayout && typeof VerticalLayout.addPane === "function") {
                            console.log("break point 1")
                            VerticalLayout.addPane(toplevelVerticalSurfaceItem)
                            console.log("break point 2")
                        } else {
                            console.error("VerticalLayout or addPane method not found")
                        }
                    } else {
                        console.error("toplevelVerticalSurfaceItem not created")
                    }
                }

                OutputLayoutItem {
                    anchors.fill: parent
                    layout: Helper.outputLayout

                    onEnterOutput: function(output) {
                        waylandSurface.surface.enterOutput(output)
                        Helper.onSurfaceEnterOutput(waylandSurface, toplevelSurfaceItem, output)
                    }
                    onLeaveOutput: function(output) {
                        waylandSurface.surface.leaveOutput(output)
                        Helper.onSurfaceLeaveOutput(waylandSurface, toplevelSurfaceItem, output)
                    }
                }

                TiledToplevelHelper {
                    id: helper
                    surface: toplevelSurfaceItem
                    waylandSurface: toplevelSurfaceItem.waylandSurface
                    creator: toplevelComponent
                }
            }
        }
    }

    // GridLayout {
    //     anchors.fill: parent
    //     columns: Math.floor(root.width / 1920 * 4)

    //     DynamicCreatorComponent {
    //         id: toplevelComponent
    //         creator: Helper.xdgShellCreator
    //         chooserRole: "type"
    //         chooserRoleValue: "toplevel"
    //         autoDestroy: false

    //         onObjectRemoved: function (obj) {
    //             obj.doDestroy()
    //         }

    //         XdgSurface {
    //             id: toplevelSurfaceItem

    //             property var doDestroy: helper.doDestroy

    //             resizeMode: SurfaceItem.SizeToSurface
    //             z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

    //             Layout.fillWidth: true
    //             Layout.fillHeight: true
    //             Layout.minimumWidth: Math.max(toplevelSurfaceItem.minimumSize.width, 100)
    //             Layout.minimumHeight: Math.max(toplevelSurfaceItem.minimumSize.height, 50)
    //             Layout.maximumWidth: toplevelSurfaceItem.maximumSize.width
    //             Layout.maximumHeight: toplevelSurfaceItem.maximumSize.height
    //             Layout.horizontalStretchFactor: 1
    //             Layout.verticalStretchFactor: 1

    //             OutputLayoutItem {
    //                 anchors.fill: parent
    //                 layout: Helper.outputLayout

    //                 onEnterOutput: function(output) {
    //                     waylandSurface.surface.enterOutput(output)
    //                     Helper.onSurfaceEnterOutput(waylandSurface, toplevelSurfaceItem, output)
    //                 }
    //                 onLeaveOutput: function(output) {
    //                     waylandSurface.surface.leaveOutput(output)
    //                     Helper.onSurfaceLeaveOutput(waylandSurface, toplevelSurfaceItem, output)
    //                 }
    //             }

    //             TiledToplevelHelper {
    //                 id: helper

    //                 surface: toplevelSurfaceItem
    //                 waylandSurface: toplevelSurfaceItem.waylandSurface
    //                 creator: toplevelComponent
    //             }
    //         }
    //     }

    //     DynamicCreatorComponent {
    //         id: popupComponent
    //         creator: Helper.xdgShellCreator
    //         chooserRole: "type"
    //         chooserRoleValue: "popup"

    //         Popup {
    //             id: popup

    //             required property WaylandXdgSurface waylandSurface
    //             property string type

    //             property alias xdgSurface: popupSurfaceItem
    //             property var parentItem: root.getSurfaceItemFromWaylandSurface(waylandSurface.parentSurface)

    //             parent: parentItem ? parentItem.item : root
    //             visible: parentItem && parentItem.item.effectiveVisible
    //                     && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
    //             x: {
    //                 let retX = 0 // X coordinate relative to parent
    //                 let minX = 0
    //                 let maxX = root.width - xdgSurface.width
    //                 if (!parentItem) {
    //                     retX = popupSurfaceItem.implicitPosition.x
    //                     if (retX > maxX)
    //                         retX = maxX
    //                     if (retX < minX)
    //                         retX = minX
    //                 } else {
    //                     retX = popupSurfaceItem.implicitPosition.x / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.x
    //                     let parentX = parent.mapToItem(root, 0, 0).x
    //                     if (retX + parentX > maxX) {
    //                         if (parentItem.type === "popup")
    //                             retX = retX - xdgSurface.width - parent.width
    //                         else
    //                             retX = maxX - parentX
    //                     }
    //                     if (retX + parentX < minX)
    //                         retX = minX - parentX
    //                 }
    //                 return retX
    //             }
    //             y: {
    //                 let retY = 0 // Y coordinate relative to parent
    //                 let minY = 0
    //                 let maxY = root.height - xdgSurface.height
    //                 if (!parentItem) {
    //                     retY = popupSurfaceItem.implicitPosition.y
    //                     if (retY > maxY)
    //                         retY = maxY
    //                     if (retY < minY)
    //                         retY = minY
    //                 } else {
    //                     retY = popupSurfaceItem.implicitPosition.y / parentItem.item.surfaceSizeRatio + parentItem.item.contentItem.y
    //                     let parentY = parent.mapToItem(root, 0, 0).y
    //                     if (retY + parentY > maxY)
    //                         retY = maxY - parentY
    //                     if (retY + parentY < minY)
    //                         retY = minY - parentY
    //                 }
    //                 return retY
    //             }
    //             padding: 0
    //             background: null
    //             closePolicy: Popup.NoAutoClose

    //             XdgSurface {
    //                 id: popupSurfaceItem
    //                 waylandSurface: popup.waylandSurface

    //                 OutputLayoutItem {
    //                     anchors.fill: parent
    //                     layout: Helper.outputLayout

    //                     onEnterOutput: function(output) {
    //                         waylandSurface.surface.enterOutput(output)
    //                         Helper.onSurfaceEnterOutput(waylandSurface, popupSurfaceItem, output)
    //                     }
    //                     onLeaveOutput: function(output) {
    //                         waylandSurface.surface.leaveOutput(output)
    //                         Helper.onSurfaceLeaveOutput(waylandSurface, popupSurfaceItem, output)
    //                     }
    //                 }
    //             }
    //         }
    //     }

    //     DynamicCreatorComponent {
    //         id: xwaylandComponent
    //         creator: Helper.xwaylandCreator
    //         autoDestroy: false

    //         onObjectRemoved: function (obj) {
    //             obj.doDestroy()
    //         }

    //         XWaylandSurfaceItem {
    //             id: xwaylandSurfaceItem

    //             required property XWaylandSurface waylandSurface
    //             property var doDestroy: helper.doDestroy

    //             shellSurface: waylandSurface
    //             resizeMode: SurfaceItem.SizeToSurface
    //             // TODO: Support popup/menu
    //             positionMode: xwaylandSurfaceItem.effectiveVisible ? XWaylandSurfaceItem.PositionToSurface : XWaylandSurfaceItem.ManualPosition
    //             z: (waylandSurface && waylandSurface.isActivated) ? 1 : 0

    //             Layout.fillWidth: true
    //             Layout.fillHeight: true
    //             Layout.minimumWidth: Math.max(xwaylandSurfaceItem.minimumSize.width, 100)
    //             Layout.minimumHeight: Math.max(xwaylandSurfaceItem.minimumSize.height, 50)
    //             Layout.maximumWidth: xwaylandSurfaceItem.maximumSize.width
    //             Layout.maximumHeight: xwaylandSurfaceItem.maximumSize.height
    //             Layout.horizontalStretchFactor: 1
    //             Layout.verticalStretchFactor: 1

    //             OutputLayoutItem {
    //                 anchors.fill: parent
    //                 layout: Helper.outputLayout

    //                 onEnterOutput: function(output) {
    //                     if (xwaylandSurfaceItem.waylandSurface.surface)
    //                         xwaylandSurfaceItem.waylandSurface.surface.enterOutput(output);
    //                     Helper.onSurfaceEnterOutput(waylandSurface, xwaylandSurfaceItem, output)
    //                 }
    //                 onLeaveOutput: function(output) {
    //                     if (xwaylandSurfaceItem.waylandSurface.surface)
    //                         xwaylandSurfaceItem.waylandSurface.surface.leaveOutput(output);
    //                     Helper.onSurfaceLeaveOutput(waylandSurface, xwaylandSurfaceItem, output)
    //                 }
    //             }

    //             TiledToplevelHelper {
    //                 id: helper

    //                 surface: xwaylandSurfaceItem
    //                 waylandSurface: surface.waylandSurface
    //                 creator: xwaylandComponent
    //             }
    //         }
    //     }
    // }

    DynamicCreatorComponent {
        id: layerComponent
        creator: Helper.layerShellCreator
        autoDestroy: false

        onObjectRemoved: function (obj) {
            obj.doDestroy()
        }

        LayerSurface {
            id: layerSurface
            creator: layerComponent
        }
    }

    DynamicCreatorComponent {
        id: inputPopupComponent
        creator: Helper.inputPopupCreator

        InputPopupSurface {
            required property WaylandInputPopupSurface popupSurface

            parent: getSurfaceItemFromWaylandSurface(popupSurface.parentSurface)
            id: inputPopupSurface
            shellSurface: popupSurface
        }
    }
}
