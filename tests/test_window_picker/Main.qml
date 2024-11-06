// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import windowpicker

Window {
    width: 400
    height: 200
    visible: true
    title: qsTr("test-window-picker")
    WindowPickerClient {
        id: windowPicker
    }
    Button {
        anchors.centerIn: parent
        text: "Pick a window"
        visible: windowPicker.ready
        onClicked: {
            windowPicker.pick()
        }
    }
    Text {
        anchors.fill: parent
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignBottom
        font.pixelSize: 30
        text: qsTr("Picked window pid:" + windowPicker.pid)
    }
}
