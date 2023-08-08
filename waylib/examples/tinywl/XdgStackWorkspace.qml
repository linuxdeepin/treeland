import QtQuick
import QtQuick.Controls
import Waylib.Server

Item {
    id: root

    property DynamicCreator creator

    function getXdgSurfaceFromWaylandSurface(surface) {
        let finder = function(props) {
            if (props.waylandSurface === surface)
                return true
        }

        let toplevel = creator.getIf(toplevelComponent, finder)
        if (toplevel) {
            return {
                parent: toplevel,
                xdgSurface: toplevel
            }
        }

        let popup = creator.getIf(popupComponent, finder)
        if (popup) {
            return {
                parent: popup,
                xdgSurface: popup.xdgSurface
            }
        }

        return null
    }

    DynamicCreatorComponent {
        id: toplevelComponent
        creator: root.creator
        chooserRole: "type"
        chooserRoleValue: "toplevel"

        XdgSurface {
            id: surface

            property bool activated: waylandSurface === Helper.activatedSurface

            visible: waylandSurface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled

            onActivatedChanged: {
                if (activated)
                    WaylibHelper.itemStackToTop(this)
            }

            Connections {
                target: waylandSurface

                function onRequestMove(seat, serial) {
                    Helper.startMove(waylandSurface, surface, surface.eventItem, seat, serial)
                }

                function onRequestResize(seat, edges, serial) {
                    Helper.startResize(waylandSurface, surface, surface.eventItem, seat, edges, serial)
                }

                function onResizeingChanged() {
                    if (waylandSurface.isResizeing)
                        surface.resizeMode = SurfaceItem.SizeToSurface
                    else
                        surface.resizeMode = SurfaceItem.SizeFromSurface
                }
            }

            Component.onCompleted: {
                forceActiveFocus()
                Helper.activatedSurface = waylandSurface
            }
        }
    }

    DynamicCreatorComponent {
        id: popupComponent
        creator: root.creator
        chooserRole: "type"
        chooserRoleValue: "popup"

        Popup { // TODO: ensure the cursor item after Popup
            id: popup

            required property WaylandXdgSurface waylandSurface
            required property OutputLayout outputLayout

            property alias xdgSurface: surface
            property var xdgParent: waylandSurface ? root.getXdgSurfaceFromWaylandSurface(waylandSurface.parentXdgSurface) : null

            parent: xdgParent ? xdgParent.parent : root
            visible: waylandSurface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
            x: surface.implicitPosition.x + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
            y: surface.implicitPosition.y + (xdgParent ? xdgParent.xdgSurface.contentItem.x : 0)
            padding: 0
            background: null

            XdgSurface {
                id: surface
                waylandSurface: popup.waylandSurface
                outputLayout: popup.outputLayout
            }

            onClosed: {
                if (waylandSurface)
                    waylandSurface.surface.unmap()
            }
        }
    }
}
