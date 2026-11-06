// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "AnimationSwitches"
    when: windowShown
    visible: true
    width: 800
    height: 600
    property var server: ({ config: { activeColor: "blue" }, animationConfig: { edgeTilePreview: true } })
    property var preview: null

    function init() {
        server = { config: { activeColor: "blue" }, animationConfig: { edgeTilePreview: true } }
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl("../../src/core/qml/EdgeTilePreview.qml"), false)
        request.send()
        verify(request.responseText.length > 0)
        // Supply the compositor configuration without starting a Wayland server.
        preview = Qt.createQmlObject(request.responseText.replace(/WaylandServer/g, "testCase.server"), testCase)
        preview.sourceGeometry = Qt.rect(0, 0, 100, 100)
        preview.targetGeometry = Qt.rect(300, 200, 400, 300)
    }

    function cleanup() {
        preview.destroy()
    }

    function disable() {
        server = { config: { activeColor: "blue" }, animationConfig: { edgeTilePreview: false } }
    }

    function verifyDestination() {
        compare(preview.x, 300)
        compare(preview.y, 200)
        compare(preview.width, 400)
        compare(preview.height, 300)
    }

    function test_disabledBeforeShow() {
        disable()
        wait(0)
        preview.visible = true
        verifyDestination()
    }

    function test_enabledBeforeShow() {
        preview.visible = true
        tryCompare(preview, "x", 300, 1000)
        verifyDestination()
    }
}
