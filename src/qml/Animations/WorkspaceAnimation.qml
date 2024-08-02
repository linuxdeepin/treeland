import QtQuick
import TreeLand.Utils

Item {
    id: root
    visible: true
    required property var workspaceManager
    readonly property int animationDuration: 400
    readonly property real refWidth: 1920
    readonly property real refGap: 30
    readonly property real refWrap: refWidth + refGap
    property int s1Id: -1
    property int s2Id: -1
    property int currentDirection: WorkspaceAnimation.Direction.Left
    property bool s1ToS2: true // Property indicates that workspace s1(to hide) is switching to s2(to show)
    property real animationInitial: 0 // Virtual scene slide initial value
    property real animationProcess: 0 // Virtual scene slide progress
    property real animationDestination: 0 // Virtual scene slide destination value
    property int initialId: -1
    property int destinationId: -1
    property real s1X: 0
    property real s2X: 0

    enum Direction {
        Left,
        Right
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    component WorkspaceShot: Item {
        id: wsShot
        property int workspaceId: -1
        required property rect sourceRect
        required property WallpaperProxy wpProxy
        width: parent.width
        height: parent.height

        ShaderEffectSource {
            id: wp
            sourceItem: wpProxy
            anchors.fill: parent
            hideSource: false
            visible: true
        }

        ShaderEffectSource {
            id: ws
            property int sourceX: 0
            visible: true
            anchors.fill: parent
            hideSource: visible
            sourceItem: workspaceManager.workspacesById.get(workspaceManager.layoutOrder.get(workspaceId)?.wsid) ?? null
            sourceRect: wsShot.sourceRect
        }
    }


    Repeater {
        model: Helper.outputLayout.outputs
        Item {
            id: animationDelegate
            visible: true
            required property var modelData
            readonly property real localAnimationScaleFactor: width / refWidth
            clip: true
            property rect displayRect: Qt.rect(modelData.x, modelData.y, modelData.width, modelData.height)
            x: displayRect.x
            y: displayRect.y
            width: displayRect.width
            height: displayRect.height

            // Wallpaper here
            WallpaperController {
                output: modelData.output
                id: wpCtrl
            }

            WorkspaceShot {
                id: s1
                sourceRect: animationDelegate.displayRect
                workspaceId: s1Id
                x: s1X * animationDelegate.localAnimationScaleFactor
                wpProxy: wpCtrl.proxy
            }
            WorkspaceShot {
                id: s2
                sourceRect: animationDelegate.displayRect
                workspaceId: s2Id
                x: s2X * animationDelegate.localAnimationScaleFactor
                wpProxy: wpCtrl.proxy
            }
        }
    }

    property real lastMod: 0
    onAnimationProcessChanged: {
        // Check if wrap
        const currMod = animationProcess % refWrap
        if ((animationProcess > 0 && currMod < lastMod) || (animationProcess < 0 && currMod > lastMod)) {
            // Wrap
            var idGap = initialId < destinationId ? 1 : -1
            if (s1ToS2 && s2Id !== destinationId) {
                s1Id = s2Id + idGap
            } else if (!s1ToS2 && s1Id !== destinationId) {
                s2Id = s1Id + idGap
            } else {
                lastMod = currMod
                return
            }
            s1ToS2 = !s1ToS2
        }
        lastMod = currMod
    }

    SequentialAnimation {
        id: slideAnimation
        alwaysRunToEnd: false
        NumberAnimation {
            target: root
            property: "animationProcess"
            from: animationInitial
            to: animationDestination
            duration: animationDuration
            easing.type: Easing.OutExpo
        }
        PropertyAction {
            target: root
            property: "visible"
            value: false
        }
    }

    function addAnimation(fromId, toId) {
        // Recalculate workspace id queue and restart animation if necessary
        var initialS1, initialS2
        if (slideAnimation.running) {
            slideAnimation.stop() // Pause animation
            console.assert((s1X < s2X) === (s1Id < s2Id), "WorkspaceShot should be continuous")
            destinationId = toId
            if (s1Id < s2Id) {
                if (toId <= s1Id) {
                    initialId = s2Id
                    currentDirection = WorkspaceAnimation.Direction.Left
                    animationInitial = s2X
                    s1ToS2 = false
                } else {
                    initialId = s1Id
                    currentDirection = WorkspaceAnimation.Direction.Right
                    animationInitial = s1X
                    s1ToS2 = true
                }
            } else {
                if (toId <= s2Id) {
                    initialId = s1Id
                    currentDirection = WorkspaceAnimation.Direction.Left
                    animationInitial = s1X
                    s1ToS2 = true
                } else {
                    initialId = s2Id
                    currentDirection = WorkspaceAnimation.Direction.Right
                    animationInitial = s2X
                    s1ToS2 = false
                }
            }
        } else {
            initialId = fromId
            destinationId = toId
            animationInitial = 0
            s1ToS2 = true
            s1Id = fromId
            s2Id = s1Id + (destinationId > initialId ? 1 : -1)
            currentDirection = (fromId < toId) ? WorkspaceAnimation.Direction.Right : WorkspaceAnimation.Direction.Left
        }
        animationDestination = -(destinationId - initialId) * refWrap
        lastMod = animationInitial % refWrap // Note: should modify last value first, otherwise modify animationProcess might cause an animation wrap
        animationProcess = animationInitial
        if (s1ToS2 && currentDirection === WorkspaceAnimation.Direction.Right) {
            initialS1 = 0
            initialS2 = refWrap
        } else if (s1ToS2 && currentDirection === WorkspaceAnimation.Direction.Left) {
            initialS1 = 0
            initialS2 = -refWrap
        } else if (!s1ToS2 && currentDirection === WorkspaceAnimation.Direction.Right) {
            initialS2 = 0
            initialS1 = refWrap
        } else {
            initialS2 = 0
            initialS1 = -refWrap
        }
        s1X = Qt.binding(function() { return (initialS1 + animationProcess + 3 * refWrap) % (2 * refWrap) - refWrap })
        s2X = Qt.binding(function() { return (initialS2 + animationProcess + 3 * refWrap) % (2 * refWrap) - refWrap })
        slideAnimation.start() // Restart animation
    }
}
