import QtQuick
import TreeLand
import TreeLand.Utils

Item {
    id: root
    visible: true
    required property var workspaceManager
    readonly property int animationDuration: 400
    readonly property int bounceDuration: 400
    readonly property real refWidth: 1920
    readonly property real refGap: 30
    readonly property real refWrap: refWidth + refGap
    readonly property real refBounce: 192
    property int s1Id: -1
    property int s2Id: -1
    property int currentDirection: WorkspaceAnimation.Direction.Left
    property bool s1ToS2: true // Property indicates that workspace s1(to hide) is switching to s2(to show)
    property int animationInitial: 0 // Virtual scene slide initial value
    property int animationProcess: 0 // Virtual scene slide progress
    property int animationDestination: 0 // Virtual scene slide destination value
    property int initialId: -1
    property int destinationId: -1
    property real s1X: 0
    property real s2X: 0
    property bool needBounce: false
    property var initialS1
    property var initialS2
    property int pendingWorkspaceId : 0

    readonly property real desktopOffset: Helper.multiTaskViewGesture.desktopOffset

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
        visible: ws.sourceItem !== null

        ShaderEffectSource {
            id: wp
            sourceItem: wpProxy
            anchors.fill: parent
            hideSource: false
            visible: true
        }

        ShaderEffectSource {
            id: ws
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

    property int lastMod: 0
    onAnimationProcessChanged: {
        if (bounceAnimation.running) return
        // Check if wrap
        const currMod = animationProcess % refWrap
        if ((animationProcess > 0 && currMod < lastMod) || (animationProcess < 0 && currMod > lastMod)) {
            // Wrap
            var idGap = initialId < destinationId ? 1 : -1
            if (s1ToS2) {
                s1Id = s2Id + idGap
            } else {
                s2Id = s1Id + idGap
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
        ScriptAction {
            script: {
                if (needBounce) {
                    bounceAnimation.start()
                } else {
                    root.visible = false
                    Helper.currentWorkspaceId = pendingWorkspaceId
                }
            }
        }
    }
    SequentialAnimation {
        id: bounceAnimation
        readonly property real bounceDestination: animationDestination + (currentDirection === WorkspaceAnimation.Direction.Left ? refBounce : -refBounce)
        NumberAnimation {
            target: root
            property: "animationProcess"
            from: animationDestination
            to: bounceAnimation.bounceDestination
            duration: bounceDuration / 2
            easing.type: Easing.InOutExpo
        }
        NumberAnimation {
            target: root
            property: "animationProcess"
            from: bounceAnimation.bounceDestination
            to: animationDestination
            duration: bounceDuration / 2
            easing.type: Easing.InOutExpo
        }
        PropertyAction {
            target: root
            property: "visible"
            value: false
        }
    }

    function animationCalc(fromId, toId) {
        needBounce = false
        if (slideAnimation.running || bounceAnimation.running) {
            animationRunning(toId)
        }
        animationNormal(fromId, toId)
    }

    function animationRunning(toId) {
        slideAnimation.stop() // Pause animation
        bounceAnimation.stop()
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
    }

    function animationNormal(fromId, toId) {
        if (gestureConnection.enable || !(slideAnimation.running || bounceAnimation.running)) {
            initialId = fromId
            destinationId = toId
            animationInitial = 0
            s1ToS2 = true
            s1Id = fromId
            s2Id = s1Id + (destinationId > initialId ? 1 : -1)
            currentDirection = (fromId < toId) ? WorkspaceAnimation.Direction.Right : WorkspaceAnimation.Direction.Left
        }
        animationPosition()
    }

    function animationPosition() {
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
    }

    function animationStart() {
        s1X = Qt.binding(function() { return (initialS1 + animationProcess + 3 * refWrap) % (2 * refWrap) - refWrap })
        s2X = Qt.binding(function() { return (initialS2 + animationProcess + 3 * refWrap) % (2 * refWrap) - refWrap })
        slideAnimation.start() // Restart animation
    }

    function addAnimation(fromId, toId) {
        if (toId < 0 || toId >= QmlHelper.workspaceManager.layoutOrder.count) {
            if (toId < 0) {
                addBounce(fromId, WorkspaceAnimation.Direction.Left)
            } else {
                addBounce(fromId, WorkspaceAnimation.Direction.Right)
            }
        } else {
            animationCalc(fromId, toId)
            animationStart()
            pendingWorkspaceId = toId
            Helper.currentWorkspaceId = pendingWorkspaceId
        }
    }

    function addBounce(currentWorkspaceId, direction) {
        if (bounceAnimation.running) return
        if (!slideAnimation.running) {
            const nWorkspaces = workspaceManager.layoutOrder.count
            destinationId = currentWorkspaceId
            currentDirection = direction
            s1Id = destinationId
            s2Id = (direction === WorkspaceAnimation.Direction.Left ? -1 : nWorkspaces)
            s1X = Qt.binding(function () { return animationProcess})
            s2X = Qt.binding(function () { return direction === WorkspaceAnimation.Direction.Left ? -refWrap + animationProcess : refWrap + animationProcess })
            animationInitial = 0
            animationProcess = 0
            animationDestination = 0
            bounceAnimation.start()
        } else {
            needBounce = true
        }
    }

    Connections {
        id: gestureConnection
        target: Helper.multiTaskViewGesture
        property bool enable: false
        property int fromId: 0
        property int toId: 0

        onDesktopOffsetChanged: {
            if (!enable) {
                enable = true
                fromId = Helper.currentWorkspaceId
                toId = 0
                if (target.desktopOffset > 0) {
                    toId = fromId + 1
                    if (toId >= QmlHelper.workspaceManager.layoutOrder.count) {
                        enable = false
                        return
                    }
                } else if (target.desktopOffset < 0) {
                    toId = fromId - 1
                    if (toId < 0) {
                        enable = false
                        return
                    }
                }
                animationNormal(fromId, toId)
            }

            if (fromId === toId)
                return

            if (enable) {
                s1X = initialS1 - desktopOffset * refWrap
                s2X = initialS2 - desktopOffset * refWrap
            }
        }

        onDesktopOffsetCancelled: {
            if (!enable)
                return

            enable = false
            if (desktopOffset === 1 || desktopOffset === -1) {
                Helper.currentWorkspaceId = toId
                return
            }

            fromId = Helper.currentWorkspaceId
            toId = 0
            if (desktopOffset > 0.25) {
                toId = fromId + 1
                if (toId >=  QmlHelper.workspaceManager.layoutOrder.count) {
                    return
                }
            } else if (desktopOffset <= -0.25) {
                toId = fromId - 1
                if (toId < 0) {
                    return;
                }
            } else {
                var temp
                temp = fromId
                fromId = toId
                toId = temp
            }

            if (fromId === toId)
                return

            animationRunning(toId)
            animationPosition(fromId, toId)
            animationStart()

            Helper.currentWorkspaceId = toId
            pendingWorkspaceId = toId
        }
    }

    Component.onCompleted: {
        pendingWorkspaceId = Helper.currentWorkspaceId
    }
}
