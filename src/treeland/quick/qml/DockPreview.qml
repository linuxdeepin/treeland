import QtQuick
import Waylib.Server
import TreeLand.Utils
import QtQuick.Controls

Item {
    id: root
    clip: true

    function getSurfaceItemFromWaylandSurface(surface) {
        let finder = function(props) {
            if (!props.waylandSurface)
                return false
            // surface is WToplevelSurface or WSurfce
            if (props.waylandSurface === surface || props.waylandSurface.surface === surface)
                return true
        }

        let toplevel = QmlHelper.xdgSurfaceManager.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                shell: toplevel,
                item: toplevel,
                type: "toplevel"
            }
        }

        let popup = QmlHelper.xdgSurfaceManager.getIf(popupComponent, finder)
        if (popup) {
            return {
                shell: popup,
                item: popup.xdgSurface,
                type: "popup"
            }
        }

        let layer = QmlHelper.layerSurfaceManager.getIf(layerComponent, finder)
        if (layer) {
            return {
                shell: layer,
                item: layer.surfaceItem,
                type: "layer"
            }
        }

        let xwayland = QmlHelper.xwaylandSurfaceManager.getIf(xwaylandComponent, finder)
        if (xwayland) {
            return {
                shell: xwayland,
                item: xwayland,
                type: "xwayland"
            }
        }

        return null
    }

    signal stopped
    signal entered(var relativeSurface)
    signal exited(var relativeSurface)
    signal surfaceActivated(Item surface)

    visible: false

    property XdgSurface currentSurface

    property var target
    property var pos
    property int direction: 0

    property int checkExited: 0
    property var isEntered: false

    function show(surfaces, target, pos, direction) {
        console.log('show',surfaces,filterModel,filterModel.sourceModel.count,filterModel.count)
        filterModel.desiredSurfaces = surfaces
        root.pos = pos;
        root.direction = direction;
        root.target = target;
        visible = true;
    }

    function close() {
        visible = false;
        stopped();
    }

    Loader {
        id: context
    }

    Component {
        id: contextComponent

        WindowsSwitcherPreview {
        }
    }

    FilterProxyModel {
        id: filterModel
        sourceModel: QmlHelper.workspaceManager.allSurfaces
        filterAcceptsRow: (data) => {
            return desiredSurfaces.some(surface => data.item.waylandSurface.surface == surface)
        }
        property var desiredSurfaces: []
        onDesiredSurfacesChanged: invalidate()
    }

    Timer {
        id: exitedTimer
        interval: 100
        onTriggered: {
            root.exited(root.target.item.surface.surface);
            context.item.stop();
            root.close()
        }
    }

    Rectangle {
        width: box.width
        height: box.height
        x: box.x
        y: box.y

        radius: 10
        opacity: 0.4
    }

    Item {
        id: box

        x: root.direction === 0 ? root.target.shell.x + root.pos.x - width / 2 :
           root.direction === 1 ? root.target.shell.x + root.pos.x - width :
           root.direction === 2 ? root.target.shell.x + root.pos.x - width / 2 :
           root.direction === 3 ? root.target.shell.x + root.target.shell.width : 0

        y: root.direction === 0 ? root.target.shell.y + root.target.shell.height :
           root.direction === 1 ? root.target.shell.y + root.pos.y - height / 2 :
           root.direction === 2 ? root.target.shell.y - height :
           root.direction === 3 ? root.target.shell.y + root.pos.y - height / 2 : 0

        width: listView.width
        height: top.height + listView.height

        HoverHandler {
            acceptedDevices: PointerDevice.AllDevices // WTF: why need allDevices?
            cursorShape: Qt.PointingHandCursor
            onHoveredChanged: {
                if (hovered) {
                    root.entered(root.target.item.surface.surface);
                }
                else {
                    exitedTimer.start();
                }
            }
        }

        Item {
            id: top
            width: title.width + closeAllBtn.width
            height: title.height
            anchors.top: parent.top
            anchors.left: parent.left

            Text {
                id: title
                width: listView.width - closeAllBtn.width
                clip: true
                verticalAlignment: Text.AlignVCenter
                font.pointSize: 15
                text: "Title"
            }

            Button {
                id: closeAllBtn
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 5
                anchors.topMargin: 5
                text: "X"
                onClicked: {
                    for (let i = 0; i < filterModel.count; i++) {
                        const item = filterModel.get(i).item
                        Helper.closeSurface(item.waylandSurface.surface)
                    }
                    exitedTimer.start();
                    closeAllBtn.visible = false // WTF: why this button cannot hide when root is hide.
                }
            }
        }

        ListView {
            id: listView
            anchors.top: top.bottom
            anchors.left: parent.left
            height: root.direction % 2 ? listView.count * 180 : 180
            width: root.direction % 2 ? 180 : listView.count * 180
            orientation: root.direction % 2 ? ListView.Vertical : ListView.Horizontal
            model: filterModel
            delegate: Item {
                required property Item item
                property Item surfaceItem: item
                width: 180
                height: 180
                clip: true
                visible: true

                Component.onCompleted: console.log('surfaceItem',surfaceItem)

                ShaderEffectSource {
                    id: effect
                    anchors.centerIn: parent
                    width: parent.width - 20
                    height: Math.min(surfaceItem.height * width / surfaceItem.width, width)
                    live: true
                    hideSource: false
                    smooth: true
                    sourceItem: surfaceItem
                    HoverHandler {
                        acceptedDevices: PointerDevice.AllDevices
                        cursorShape: Qt.PointingHandCursor
                        onHoveredChanged: {
                            if (hovered) {
                                root.isEntered = true;
                                closeBtn.visible = true
                                closeAllBtn.visible = false
                                title.text = surfaceItem.waylandSurface.title

                                context.parent = root.parent;
                                context.anchors.fill = root;
                                context.sourceComponent = contextComponent;
                                context.item.start(surfaceItem);
                            }
                            else {
                                title.text = "Title"
                                closeBtn.visible = false
                                closeAllBtn.visible = true
                                context.item.stop()
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            surfaceActivated(surfaceItem);
                        }
                    }

                    Button {
                        id: closeBtn
                        visible: false
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: 5
                        anchors.topMargin: 5
                        text: "X"
                        onClicked: {
                            Helper.closeSurface(surfaceItem.waylandSurface.surface);
                        }
                    }
                }
            }
        }
    }
}
