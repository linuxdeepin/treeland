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
    required property OutputDelegate activeOutput
    // required property var allWins

    signal surfaceActivated(surface: XdgSurface)

    onVisibleChanged: {
        if (visible) {
            current = 0
            indicatorPlane.calcLayout()
            next()
            // console.log('liveview',allWins,allWins.width,allWins.height,workspaceLiveView.width,workspaceLiveView.height,workspaceLiveView.z)
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
    }
    function next() {
        current = current + 1
        if (current >= model.count) {
            current = 0
        }
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
        // adjust win stack
        if(current!=0) {
            console.log('adjust',current,'to first')
            model.move(current,0,1)
        }
    }
    
    onCurrentChanged: show()

    // invisible impl, makes cursor style also not changed
    // ShaderEffectSource {
    //     id: workspaceLiveView
    //     anchors{
    //         fill: parent
    //         margins: 50
    //     }
    //     live: true
    //     hideSource: false
    //     smooth: true
    //     sourceItem: allWins
    //     // opacity: .3
    // }

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
        onCountChanged: if(visible)indicatorPlane.calcLayout()
    }

    property int spacing: 10
    property var rows: []
    property int rowHeight: 0
    property int padding: 8
    onRowsChanged: console.log('rows', rows.length, rowHeight)

    Item {
        id: indicatorPlane
        
        // currently use binding, so indicatorPlane follows mouse/active output
        x: {
            const coord=parent.mapFromItem(activeOutput,0,0)
            console.log('coord',coord,width,height)
            return coord.x
        }
        y: {
            const coord=parent.mapFromItem(activeOutput,0,0)
            return coord.y
        }
        width: activeOutput?.width
        height: activeOutput?.height
        Component.onCompleted: console.log('box hw',activeOutput,width,height)
        
        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log('out')
                root.visible = false
            }
        }

        Rectangle {
            width: flickable.width + 2 * padding
            height: flickable.height + 2 * padding
            anchors.centerIn: flickable
            radius: 10
            opacity: 0.4
        }

        Flickable {
            id: flickable
            width: eqhgrid.width
            height: Math.min(eqhgrid.height, parent.height * 0.7)
            anchors.centerIn: parent
            contentHeight: eqhgrid.height
            boundsBehavior: Flickable.StopAtBounds
            Behavior on contentY {
                NumberAnimation {
                    duration: 200
                }
            }
            clip: true

            ColumnLayout {
                id: eqhgrid // equal height grid
                anchors.centerIn: parent
                Component.onCompleted: console.log('eqhgrid',mapToGlobal(0,0),width,height,rows)
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
                                property bool highlighted: globalIdx == root.current 
                                width: modelData.dw + 2 * padding
                                height: col.height + 2 * padding
                                property int globalIdx: index + parent.baseIdx
                                Component.onCompleted: console.log('item', modelData,
                                                                index, globalIdx,height,width)
                                border.color: "blue"
                                border.width: highlighted ? 2 : 0
                                color: Qt.rgba(255,255,255,.2)
                                radius: 8
                                onHighlightedChanged: {
                                    // auto scroll to current highlight
                                    if (highlighted) {
                                        console.log('highlighted',
                                                    flickable.contentY,
                                                    mapToItem(flickable, 0, 0),
                                                    mapFromItem(flickable, 0, 0))
                                        flickable.contentY = Math.min(
                                                    Math.max(
                                                        mapToItem(
                                                            eqhgrid, 0,
                                                            height).y - flickable.height,
                                                        flickable.contentY),
                                                    mapToItem(eqhgrid, 0, 0).y)
                                    }
                                }
                                Column {
                                    id: col
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: root.padding
                                    }
                                    spacing: 5

                                    RowLayout {
                                        width: parent.width
                                        Rectangle {
                                            height: width
                                            width: 24
                                            color: "yellow"
                                            radius: 5
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: {
                                                const xdg=source.waylandSurface
                                                const wholeTitle=xdg.appId?.length?`${xdg.title} - ${xdg.appId}`:xdg.title
                                                console.log(wholeTitle)
                                                wholeTitle
                                            }
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
                                            hideSource: false
                                            smooth: true
                                            sourceItem: source
                                        }
                                        Component.onCompleted: console.log('thumb',source)
                                    }
                                }
                                TapHandler {
                                    onTapped: {
                                        console.log('tapped idx',globalIdx)
                                        root.current = globalIdx
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        function calcLayout() {
            var minH = 100, maxH = 200, maxW = indicatorPlane.width * maxH / indicatorPlane.height, totMaxWidth = indicatorPlane.width * 0.7
            function tryLayout(rowH, div) {
                var nrows = 1
                var acc = 0
                var rowstmp = []
                var currow = []
                for (var i = 0; i < model.count; i++) {
                    var win = model.get(i)
                    var ratio = win.source.width / win.source.height
                    var curW = Math.min(maxW, ratio * rowH)
                    console.log('curW', curW)
                    var wwin = {
                        "dw": curW
                    }
                    Object.assign(wwin, win)
                    acc += curW
                    if (acc <= totMaxWidth)
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
                    console.log('calcover', nrows, rowH,rows)
                    return true
                }
                return false
            }

            for (var div = 1; indicatorPlane.height / div >= minH; div++) {
                // return if width satisfies
                console.log('div=', div)
                var rowH = Math.min(indicatorPlane.height / div, maxH)
                if (tryLayout(rowH, div))
                    return
            }
            tryLayout(minH, 999)
            console.warn('cannot layout')
        }
    }
}
