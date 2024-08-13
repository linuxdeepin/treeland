// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property int minH
    required property int maxH
    required property int maxW
    required property int availH
    required property int availW
    required property var model
    required property Component delegate
    
    property int itemVerticalPadding: 0
    property int spacing: 8
    property bool exited: false
    property bool animationFinished: false

    property real partialGestureFactor: effect.partialGestureFactor
    property bool inProgress: effect.inProgress
    property var activeMethod: MultitaskView.ActiveMethod.ShortcutKey

    property var pos: []

    Repeater {
        model: root.model
        Loader {
            id: surfaceLoader
            required property int index
            required property var modelData
            property var internalData: pos[index]
            property int globalIndex: index
            property real displayWidth: modelData.item.width
            property bool initialized: false
            property bool activated: !animationFinished && initialized && pos.length > index && activeMethod === MultitaskView.ActiveMethod.ShortcutKey
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
                        displayWidth: modelData.item.width
                    }
                },
                State {
                    name: "partial"
                    PropertyChanges {
                        target: surfaceLoader
                        x: (internalData.dx - initialPos.x) * partialGestureFactor + initialPos.x
                        y: (internalData.dy - initialPos.y) * partialGestureFactor + initialPos.y
                        displayWidth: (internalData.dw - modelData.item.width) * partialGestureFactor + modelData.item.width
                    }
                },
                State {
                    name: "taskview"
                    PropertyChanges {
                        target: surfaceLoader
                        x: internalData.dx
                        y: internalData.dy
                        displayWidth: internalData.dw
                    }
                }
            ]
            state: {
                if (exited)
                    return "initial";

                if (activeMethod === MultitaskView.ActiveMethod.ShortcutKey){
                    if (animationFinished)
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
                    NumberAnimation {
                        properties: "x, y, displayWidth"
                        duration: 250
                        easing.type: Easing.OutCubic
                    }
                }
            ]

            onInternalDataChanged: if (internalData && internalData.dw && initialized) {
                x = internalData.dx
                y = internalData.dy
                displayWidth = internalData.dw
            }
            Behavior on x { enabled: activated; NumberAnimation { duration: 250; easing.type: Easing.OutCubic }}
            Behavior on y { enabled: activated; NumberAnimation { duration: 250; easing.type: Easing.OutCubic }}
            Behavior on displayWidth {
                enabled: activated
                SequentialAnimation {
                    NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                    ScriptAction {
                        script: animationFinished = true
                    }
                }
            }
            Component.onCompleted: {
                initialized = true
            }
        }
        // caution: repeater's remove may happen after calclayout, so last elem got null and some got wrong sourceitem
        onItemAdded: calcLayout()
        onItemRemoved: calcLayout()
    }
    property var getRatio: (d)=>d.source.width / d.source.height
    function calcLayout() {
        let rows = []
        let rowHeight = 0
        function tryLayout(rowH, div) {
            var nrows = 1
            var acc = 0
            var rowstmp = []
            var currow = []
            for (var i = 0; i < model.count; i++) {
                var win = model.get(i)
                var ratio = getRatio(win)
                var curW = Math.min(maxW, ratio * rowH)
                const displayInfo = {dw: curW}
                acc += curW + root.spacing
                if (acc <= availW)
                    currow.push(displayInfo)
                else {
                    acc = curW
                    nrows++
                    rowstmp.push(currow)
                    currow = [displayInfo]
                    if (nrows > div)
                        break
                }
            }
            if (nrows <= div) {
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
            let curY = 0
            const maxW = rows.reduce((acc, row) => Math.max(acc, row.reduce( (acc, it) => it.dw + acc + root.spacing, -root.spacing )), 0)
            root.width = maxW
            const hCenter = root.width / 2
            for (let row of rows) {
                const totW = row.reduce((acc, it) => it.dw + acc + root.spacing, -root.spacing)
                const left = hCenter - totW / 2
                row.reduce((acc, it) => {
                    Object.assign(it, {dx: acc, dy: curY})
                    postmp.push(it)
                    return it.dw + acc + root.spacing
                }, left)
                curY += rowHeight + itemVerticalPadding + root.spacing
            }
            pos = postmp
            root.height = curY - root.spacing
            return
        }
        for (var div = 1; availH / div >= minH; div++) {
            // return if width satisfies
            var rowH = Math.min(availH / div, maxH)
            if (tryLayout(rowH, div)) {
                calcDisplayPos()
                return
            }
        }
        if (tryLayout(minH, 999)) {
            calcDisplayPos()
        } else {
            console.warn('cannot layout')
        }
    }
}
