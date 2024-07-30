import QtQuick

Item {
    id: root
    required property var workspaceManager

    visible: false

    enum Direction {
        Left,
        Right
    }

    component WorkspaceShot: ShaderEffectSource {
        property int workspaceId: -1
        id: ws
        property int sourceX: 0

        x: 0
        y: 0
        width: parent.width
        height: parent.height
        sourceRect {
            x: ws.sourceX
            y: 0
            width: ws.width
            height: ws.height
        }
        transitions: [
            Transition {
                reversible: true
                NumberAnimation {
                    target: ws
                    duration: 250
                    properties: "x,width,sourceX"
                    easing.type: Easing.OutCubic
                }
            }
        ]
        hideSource: visible
        sourceItem: workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(workspaceId)?.wsid) ?? null
        states: [
            State {
                name: "left"
                PropertyChanges {
                    ws {
                        width: 0
                        sourceX: parent.width
                    }
                }
            },
            State {
                name: "right"
                PropertyChanges {
                    ws {
                        width: 0
                        x: parent.width
                    }
                }
            },
            State {
                name: "center"
                PropertyChanges {
                    ws {
                        width: parent.width
                        x: 0
                        sourceX: 0
                    }
                }
            }
        ]

        // FIXME: Do we need to explicitly assign the initial value to sourceRect.width
        function leftPrepare() {
            width = 0
            sourceX = parent.width
        }

        function centerPrepare() {
            width = parent.width
            x = 0
            sourceX = 0
        }

        function rightPrepare() {
            width = 0
            x = parent.width
        }
    }

    WorkspaceShot {
        id: toRemove
    }

    WorkspaceShot {
        id: toDisplay
    }

    Timer {
        id: deactivateTimer
        interval: 250
        repeat: false
        onTriggered: {
            root.visible = false
        }
    }

    function startAnimation(displayId, removeId, direction) {
        toDisplay.workspaceId = displayId
        toRemove.workspaceId = removeId
        switch (direction) {
        case WorkspaceAnimation.Direction.Left:
            toDisplay.leftPrepare()
            toRemove.centerPrepare()
            root.visible = true
            toDisplay.state = "center"
            toRemove.state = "right"
            break
        case WorkspaceAnimation.Direction.Right:
            toDisplay.rightPrepare()
            toRemove.centerPrepare()
            root.visible = true
            toRemove.state = "left"
            toDisplay.state = "center"
            break
        default:
            break
        }
        deactivateTimer.start()
    }
}
