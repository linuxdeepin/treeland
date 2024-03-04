// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
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
    required property ListModel model
    required property Component delegate
    
    property int rowHeight: 0
    property int padding: 8
    property var rows: []
    spacing: 10
    onRowsChanged: console.log('rows', rows.length, rowHeight)

    Component.onCompleted: console.log('eqhgrid',availH,availW, mapToGlobal(0, 0), width,
                                       height, rows, delegate)

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
                console.log('curW', curW,ratio)
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
                    console.log('cur', currow, 'tmp', rowstmp)
                    rowstmp.push(currow)
                    currow = [wwin]
                    if (nrows > div)
                        break
                }
                console.info(acc)
            }
            if (nrows <= div) {
                if (currow.length)
                    rowstmp.push(currow)
                rowHeight = rowH
                rows = rowstmp
                console.log('calcover', nrows, rowH, rows)
                return true
            }
            return false
        }

        for (var div = 1; availH / div >= minH; div++) {
            // return if width satisfies
            console.log('div=', div,availH,availW,maxH,maxW)
            var rowH = Math.min(availH / div, maxH)
            if (tryLayout(rowH, div))
                return
        }
        tryLayout(minH, 999)
        console.warn('cannot layout')
    }
}
