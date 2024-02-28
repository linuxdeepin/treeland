// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import Waylib.Server
import TreeLand

Item {
    id: root
    property alias model: model
    property var current: 0

    signal surfaceActivated(surface: XdgSurface)

    onVisibleChanged: {
        if (visible) {
            calcLayout()
            next()
        }
        else {
            stop()
        }
    }

    function previous() {
        current = current - 1
        if (current < 0) {
            current = 0
        }

        show();
    }
    function next() {
        current = current + 1
        if (current >= model.count) {
            current = 0
        }

        show();
    }

    function show() {
        if (model.count < 1) {
            return
        }

        const source = model.get(current).source

        context.parent = parent
        context.anchors.fill = root
        context.sourceComponent = contextComponent
        context.item.start(source)
        surfaceActivated(source)
    }

    function stop() {
        if (context.item) {
            context.item.stop()
        }
    }

    Loader {
        id: context
    }

    Component {
        id: contextComponent

        WindowsSwitcherPreview {
        }
    }

    ListModel {
        id: model
        function removeSurface(surface) {
            for (var i = 0; i < model.count; i++) {
                if (model.get(i).source === surface) {
                    model.remove(i);
                    break;
                }
            }
        }
        // onCountChanged: calcLayout()
    }
    // Component.onCompleted: calcLayout()
    property int spacing: 10
    property var rows: []
    property int rowHeight: 0
    property int padding: 8
    onRowsChanged: console.log('rows', rows.length, rowHeight)
    ColumnLayout {
        id: eqhgrid // equal height grid
        anchors.centerIn: parent
        Repeater {
            model: rows
            RowLayout {
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
                    Rectangle {
                        property XdgSurface source: modelData.source
                        width: source.width / source.height * rowHeight + 2 * root.padding
                        height: col.height + 2 * root.padding
                        property int globalIdx: index + parent.baseIdx
                        Component.onCompleted: console.log('item', modelData,
                                                           index, globalIdx)
                        border.color: "blue"
                        border.width: globalIdx == root.current ? 2 : 0
                        radius: 8
                        Column {
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                                margins: root.padding
                            }
                            id: col
                            RowLayout {
                                width: parent.width
                                Rectangle {
                                    height: width
                                    width: 24
                                    color: "yellow"
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "test title ttttttttttttttttttttttt"
                                    elide: Qt.ElideRight
                                }
                            }
                            Item {
                                id: thumb
                                width: parent.width
                                height: source.height * width / source.width
                                clip: true
                                visible: true
                                ShaderEffectSource {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: source.height * width / source.width
                                    live: true
                                    hideSource: visible
                                    smooth: true
                                    sourceItem: source
                                }
                                Component.onCompleted: console.log('thumb',source)
                            }
                        }
                    }
                }
            }
        }
    }

    function calcLayout() {
        console.warn('calcLayout')
        var minH = 100, maxH = 200
        function tryLayout(rowH, div) {
            var nrows = 1
            var acc = 0
            var rowstmp = []
            var currow = []
            for (var i = 0; i < model.count; i++) {
                var win = model.get(i)
                console.log('win',i,'=',win,win.source,win.source.width)
                var ratio = win.source.width / win.source.height
                acc += ratio
                if (acc * rowH <= root.width)
                    currow.push(win)
                else {
                    acc = ratio
                    nrows++
                    console.log('cur', currow, 'tmp', rowstmp)
                    rowstmp.push(currow)
                    currow = [win]
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
                console.log('calcover', nrows, rowH)
                return true
            }
            return false
        }

        for (var div = 1; root.height / div >= minH; div++) {
            // return if width satisfies
            console.log('div=', div)
            var rowH = Math.min(root.height / div, maxH)
            if (tryLayout(rowH, div))
                return
        }
        tryLayout(minH, 999)
        console.warn('cannot layout')
    }

    Rectangle {
        width: eqhgrid.width
        height: eqhgrid.height
        anchors.centerIn: parent
        radius: 10
        opacity: 0.4
    }

    // Row {
    //     anchors.centerIn: parent
    //     id: switcher
    //     Repeater {
    //         model: model
    //         
    //     }
    // }
}
