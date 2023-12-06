// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import TreeLand

Item {
    id: root
    property alias model: model
    property var current: 0

    signal surfaceActivated(surface: XdgSurface)

    onVisibleChanged: {
        if (visible) {
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
    }

    Rectangle {
        width: switcher.width
        height: switcher.height
        anchors.centerIn: parent
        radius: 10
        opacity: 0.4
    }

    Row {
        anchors.centerIn: parent
        id: switcher
        Repeater {
            model: model
            Item {
                required property XdgSurface source
                width: 180
                height: 160
                clip: true
                visible: true
                ShaderEffectSource {
                    anchors.centerIn: parent
                    width: 150
                    height: Math.min(source.height * width / source.width, width)
                    live: true
                    hideSource: visible
                    smooth: true
                    sourceItem: source
                }
            }
        }
    }
}
