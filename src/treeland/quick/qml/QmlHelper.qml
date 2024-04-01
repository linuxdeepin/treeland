// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton

import QtQuick
import Waylib.Server

Item {
    property OutputLayout layout: OutputLayout {}
    property alias outputManager: outputManager
    property DynamicCreator xdgSurfaceManager: xdgSurfaceManager
    property DynamicCreator layerSurfaceManager: layerSurfaceManager
    property DynamicCreator xwaylandSurfaceManager: xwaylandSurfaceManager
    property DynamicCreator inputPopupSurfaceManager: inputPopupSurfaceManager
    property alias shortcutManager: shortcutManager
    property alias workspaceManager: workspaceManager

    function printStructureObject(obj) {
        var json = ""
        for (var prop in obj){
            if (!obj.hasOwnProperty(prop)) {
                continue;
            }
            let value = obj[prop]
            try {
                json += `    ${prop}: ${value},\n`
            } catch (err) {
                json += `    ${prop}: unknown,\n`
            }
        }

        return '{\n' + json + '}'
    }

    DynamicCreator {
        id: outputManager
        onObjectAdded: function(delegate, obj, properties) {
            console.info(`New output item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }

        onObjectRemoved: function(delegate, obj, properties) {
            console.info(`Output item ${obj} is removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    DynamicCreator {
        id: xdgSurfaceManager
        onObjectAdded: function(delegate, obj, properties) {
            console.info(`New Xdg surface item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }

        onObjectRemoved: function(delegate, obj, properties) {
            console.info(`Xdg surface item ${obj} is removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    DynamicCreator {
        id: layerSurfaceManager
        onObjectAdded: function(delegate, obj, properties) {
            console.info(`New Layer surface item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }

        onObjectRemoved: function(delegate, obj, properties) {
            console.info(`Layer surface item ${obj} is removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    DynamicCreator {
        id: xwaylandSurfaceManager
        onObjectAdded: function(delegate, obj, properties) {
            console.info(`New X11 surface item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }

        onObjectRemoved: function(delegate, obj, properties) {
            console.info(`X11 surface item ${obj} is removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    DynamicCreator {
        id: inputPopupSurfaceManager
        onObjectAdded: function (delegate, obj, properties) {
            console.info(`New input popup surface item ${obj} from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }

        onObjectRemoved: function (delegate, obj, properties) {
            console.info(`Input popup surface item ${obj} is removed, it's create from delegate ${delegate} with initial properties:`,
                         `\n${printStructureObject(properties)}`)
        }
    }

    QtObject {
        id: shortcutManager
        signal screenLocked
        signal multitaskViewToggled
        signal nextWorkspace
        signal prevWorkspace
        signal moveToNeighborWorkspace(d: int)
    }
    QtObject {
        id: workspaceManager
        property var workspacesById: new Map()
        property alias nWorkspaces: layoutOrder.count
        property var __: QtObject {
            id: privt
            property int workspaceIdCnt: { workspaceIdCnt = layoutOrder.count - 1 }
        }
        function createWs() {
            layoutOrder.append({ wsid: ++privt.workspaceIdCnt })
        }
        function destroyWs(id) {
            console.log('destroyws',id)
            layoutOrder.remove(id)
            console.log(layoutOrder)
        }
        function moveWs(from, to) {
            layoutOrder.move(from, to, 1)
        }

        property ListModel layoutOrder: ListModel {
            id: layoutOrder
            objectName: "layoutOrder"
            ListElement {
                wsid: 0
            }
        }
        property ListModel allSurfaces: ListModel {
            id: allSurfaces
            objectName: "allSurfaces"
            function removeIf(cond) {
                for (let i = 0; i < this.count; i++) {
                    if (cond(this.get(i))) {
                        this.remove(i)
                        return
                    }
                }
            }
        }
        Component.onCompleted: {
            createWs(),createWs()
        }
    }
}
