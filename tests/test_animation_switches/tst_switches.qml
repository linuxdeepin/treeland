// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtTest
import Treeland

TestCase {
    id: testCase
    name: "AnimationSwitches"
    when: windowShown
    visible: true
    width: 800
    height: 600
    Component {
        id: animationConfigComponent
        QtObject {
            property bool edgeTilePreview: true
        }
    }
    Component {
        id: previewComponent
        EdgeTilePreview {}
    }
    property var preview: null

    function init() {
        failOnWarning(/.*/)
        WaylandServer.animationConfig = createTemporaryObject(animationConfigComponent, testCase)
        preview = previewComponent.createObject(testCase)
        verify(preview !== null)
        preview.sourceGeometry = Qt.rect(0, 0, 100, 100)
        preview.targetGeometry = Qt.rect(300, 200, 400, 300)
    }

    function cleanup() {
        if (preview) {
            preview.destroy()
            preview = null
            wait(0)
        }
        WaylandServer.animationConfig = null
    }

    function disable() {
        WaylandServer.animationConfig.edgeTilePreview = false
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
        verify(preview.x < 300)
        tryCompare(preview, "x", 300, 1000)
        verifyDestination()
    }

    function test_configurationReplacement() {
        preview.visible = true
        tryCompare(preview, "x", 300, 1000)
        WaylandServer.animationConfig = createTemporaryObject(
            animationConfigComponent, testCase, {edgeTilePreview: false})
        tryCompare(preview, "animationAllowed", false)
        preview.targetGeometry = Qt.rect(400, 300, 500, 400)
        compare(preview.x, 400)
        compare(preview.y, 300)
        compare(preview.width, 500)
        compare(preview.height, 400)
    }

    function test_reenabledForNextShow() {
        disable()
        preview.visible = true
        verifyDestination()
        preview.visible = false
        WaylandServer.animationConfig.edgeTilePreview = true
        tryCompare(preview, "animationAllowed", true)
        preview.visible = true
        verify(preview.x < 300)
        tryCompare(preview, "x", 300, 1000)
        verifyDestination()
    }

}
