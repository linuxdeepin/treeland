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
        id: userConfigComponent
        QtObject {
            readonly property color activeColor: "blue"
            property int edgeTilePreview: 3
        }
    }
    Component {
        id: previewComponent
        EdgeTilePreview {}
    }
    property var preview: null

    function init() {
        failOnWarning(/.*/)
        WaylandServer.effectLevel = 3
        WaylandServer.config = createTemporaryObject(userConfigComponent, testCase)
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
        WaylandServer.config = null
        WaylandServer.effectLevel = 3
    }

    function disable() {
        WaylandServer.config.edgeTilePreview = 0
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
        WaylandServer.config = createTemporaryObject(
            userConfigComponent, testCase, {edgeTilePreview: 0})
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
        WaylandServer.config.edgeTilePreview = 3
        tryCompare(preview, "animationAllowed", true)
        preview.visible = true
        verify(preview.x < 300)
        tryCompare(preview, "x", 300, 1000)
        verifyDestination()
    }

    function test_effectLevelSwitch() {
        WaylandServer.effectLevel = 2
        tryCompare(preview, "animationAllowed", false)
        preview.visible = true
        verifyDestination()
        preview.visible = false
        WaylandServer.effectLevel = 3
        tryCompare(preview, "animationAllowed", true)
        preview.visible = true
        verify(preview.x < 300)
        tryCompare(preview, "x", 300, 1000)
        verifyDestination()
    }

}
