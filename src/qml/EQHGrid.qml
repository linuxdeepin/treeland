// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Layouts

Flickable {
    id: root
    signal requestExit()
    readonly property int minWindowHeight: 232
    readonly property int maxWindowHeight: Math.min(720, availHeight) // Note: This might not be respected cause we should consider margins and let window always visible
    readonly property int windowHeightStep: 15
    readonly property int topContentMargin: 40
    readonly property int bottomContentMargin: 60
    readonly property int horizontalContentMargin: 20
    readonly property int availWidth: width - 2 * horizontalContentMargin
    readonly property int availHeight: height - topContentMargin - bottomContentMargin
    readonly property int cellPadding: 12 // Padding for each element
    readonly property bool windowLoaded: windowLoader.status === Loader.Ready

    required property var model
    required property Component delegate
    readonly property int contentMargin: 40 // preferred content margin
    readonly property alias delegateCornerRadius: ros.delegateCornerRadius
    required property int animationDuration
    required property int animationEasing
    property real paddingOpacity: 0.1
    readonly property alias layoutReady: ros.layoutReady

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            requestExit()
        }
    }

    QtObject {
        id: ros // readonly state
        property bool layoutReady: false
        property list<real> cornerRadiusList: [18,12,8] // Should get from system preference
        property int rows: 0
        property real delegateCornerRadius: (rows >= 1 && rows <= 3) ? cornerRadiusList[rows - 1] : cornerRadiusList[2]
    }

    property bool exited: false
    property bool animationFinished: false
    property real partialGestureFactor: effect.partialGestureFactor
    property bool inProgress: effect.inProgress
    property int activeReason: MultitaskView.ActiveReason.ShortcutKey

    flickableDirection: Flickable.AutoFlickIfNeeded
    clip: animationFinished && !exited
    property var pos: []

    Loader {
        id: windowLoader
        sourceComponent: Repeater {
            model: root.model
            Loader {
                id: surfaceLoader
                required property int index
                required property var modelData
                property var internalData: pos[index]
                property real displayWidth: modelData.item.width
                property real displayHeight: modelData.item.height
                property real paddingOpacity
                property bool needPadding
                sourceComponent: root.delegate
                readonly property point initialPos: mapToItem(parent, mapFromItem(modelData.item, 0, 0))
                z: -index

                states: [
                    State {
                        name: "initial"
                        PropertyChanges {
                            target: surfaceLoader
                            x: initialPos.x
                            y: initialPos.y
                            width: modelData.item.width
                            height: modelData.item.height
                            paddingOpacity: 0
                            needPadding: false
                        }
                    },
                    State {
                        name: "partial"
                        PropertyChanges {
                            target: surfaceLoader
                            x: (internalData.dx - initialPos.x) * partialGestureFactor + initialPos.x
                            y: (internalData.dy - initialPos.y) * partialGestureFactor + initialPos.y
                            width: (internalData.dw - modelData.item.width) * partialGestureFactor + modelData.item.width
                            height: (internalData.dh - modelData.item.height) * partialGestureFactor + modelData.item.height
                            paddingOpacity: root.paddingOpacity * partialGestureFactor
                            needPadding: true
                        }
                    },
                    State {
                        name: "taskview"
                        PropertyChanges {
                            target: surfaceLoader
                            x: internalData.dx
                            y: internalData.dy
                            width: internalData.dw
                            height: internalData.dh
                            needPadding: internalData.np
                            paddingOpacity: root.paddingOpacity
                        }
                    }
                ]
                state: {
                    if (exited)
                        return "initial";

                    if (activeReason === MultitaskView.ActiveReason.ShortcutKey && layoutReady){
                        return "taskview";
                    } else {
                        if (root.inProgress) return "partial";

                        if (root.partialGestureFactor > 0.5) return "taskview";
                    }
                    return "initial";
                }
                transitions: [
                    Transition {
                        to: "initial, taskview"
                        ParallelAnimation {
                            NumberAnimation {
                                properties: "x, y, width, height, paddingOpacity"
                                duration: animationDuration
                                easing.type: animationEasing
                            }
                            PropertyAction {
                                target: surfaceLoader
                                property: "needPadding"
                            }
                        }
                    }
                ]
            }
            // caution: repeater's remove may happen after calclayout, so last elem got null and some got wrong sourceitem
            onItemAdded: if (windowLoaded) calcLayout()
            onItemRemoved: if (windowLoaded) calcLayout()
        }
    }

    property var getRatio: (d)=>d.source.width / d.source.height
    function calcLayout() {
        ros.layoutReady = false
        let rows = []
        let rowHeight = 0
        function tryLayout(rowH, ignoreOverlap = false) {
            const loadFactor = 0.6 // Every line must at least occupy certain portion of width
            var nrows = 1
            var acc = 0
            var rowstmp = []
            var currow = []
            for (var i = 0; i < model.count; i++) {
                var win = model.get(i)
                var ratio = getRatio(win)
                var needPadding = win.item.height < (rowH - 2 * cellPadding)
                var curW = Math.min(availWidth, ratio * Math.min(rowH - 2 * cellPadding, win.item.height) + 2 * cellPadding)
                const displayInfo = {dw: curW - 2 * cellPadding, np: needPadding}
                const newAcc = acc + curW
                if (newAcc <= availWidth) {
                    acc = newAcc
                    currow.push(displayInfo)
                } else if (newAcc / availWidth > loadFactor) {
                    acc = curW
                    nrows++
                    rowstmp.push(currow)
                    currow = [displayInfo]
                } else {
                    // Just scale the last element
                    curW = availWidth - acc
                    currow.push({dw: curW - 2 * cellPadding, np: needPadding})
                    acc = newAcc
                }
            }
            if (nrows * rowH <= availHeight || ignoreOverlap) {
                if (currow.length)
                    rowstmp.push(currow)
                rowHeight = rowH
                rows = rowstmp
                return true
            }
            return false
        }
        function calcDisplayPos() {
            let postmp = []
            let contentHeight = rows.length * rowHeight
            ros.rows = rows.length
            let curY = Math.max(root.availHeight - contentHeight, 0) / 2 + root.topContentMargin // If content's height is smaller than available height, let content be aligned center
            const hCenter = root.width / 2
            for (let row of rows) {
                const totW = row.reduce((acc, it) => it.dw + 2 * cellPadding + acc, 0)
                const left = hCenter - totW / 2 + cellPadding
                row.reduce((acc, it) => {
                    Object.assign(it, {dx: acc, dy: curY + cellPadding, dh: rowHeight - 2 * cellPadding})
                    postmp.push(it)
                    return it.dw + acc + 2 * cellPadding
                }, left)
                curY += rowHeight
            }
            pos = postmp
            root.contentHeight = curY
            return
        }
        var rowH
        for (rowH = maxWindowHeight; rowH > minWindowHeight; rowH -= windowHeightStep) {
            if (tryLayout(rowH)) {
                break
            }
        }
        if (rowH < minWindowHeight) {
            rowH = minWindowHeight
            tryLayout(minWindowHeight, true)
        }
        calcDisplayPos()
        ros.layoutReady = true
    }
    Component.onCompleted: {
        calcLayout()
    }
}
