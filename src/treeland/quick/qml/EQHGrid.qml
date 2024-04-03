// Copyright (C) 2024 Yicheng Zhong <zhongyicheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: eqhgrid // equal height grid

    required property int minH
    required property int maxH
    required property int maxW
    required property int availH
    required property int availW
    required property var model
    required property Component delegate
    
    property int rowHeight: 0
    property int padding: 8
    property var rows: []
    spacing: 10

    Repeater {
        model: rows
        RowLayout {
            id: row
            width: eqhgrid.width
            Layout.alignment: Qt.AlignHCenter
            property int baseIdx: {
                var idx = 0
                for (var i = 0; i < index; i++)
                    idx += rows[i].length
                return idx
            }
            Repeater {
                model: modelData
                delegate: Loader {
                    required property int index
                    required property var modelData
                    property int globalIndex: index + row.baseIdx
                    sourceComponent: eqhgrid.delegate
                }
            }
        }
    }
    property var getRatio: (d)=>d.source.width / d.source.height
    function calcLayout() {
        function tryLayout(rowH, div) {
            var nrows = 1
            var acc = 0
            var rowstmp = []
            var currow = []
            for (var i = 0; i < model.count; i++) {
                var win = model.get(i)
                var ratio = getRatio(win)
                var curW = Math.min(maxW, ratio * rowH)
                var wwin = {
                    "dw": curW
                }
                Object.assign(wwin, win)
                acc += curW
                if (acc <= availW)
                    currow.push(wwin)
                else {
                    acc = curW
                    nrows++
                    rowstmp.push(currow)
                    currow = [wwin]
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

        for (var div = 1; availH / div >= minH; div++) {
            // return if width satisfies
            var rowH = Math.min(availH / div, maxH)
            if (tryLayout(rowH, div))
                return
        }
        tryLayout(minH, 999)
        console.warn('cannot layout')
    }
}
