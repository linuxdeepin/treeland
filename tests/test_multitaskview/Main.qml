// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import test.multitaskview

Window {
    width: 400
    height: 200
    visible: true
    title: qsTr("test-multitaskview")
    Multitaskview {
        id: multitaskview
    }
    Timer {
        id: timer
        interval: 5000
        onTriggered: {
            multitaskview.toggle()
        }
    }
    ColumnLayout {
        anchors.fill: parent
        Button {
            text: "Show multitaskview with hide timer(5s)"
            visible: multitaskview.ready
            onClicked: {
                multitaskview.toggle()
                timer.restart()
            }
            Layout.alignment: Qt.AlignCenter
        }
        Button {
            text: "Show multitaskview"
            visible: multitaskview.ready
            onClicked: {
                multitaskview.toggle()
            }
            Layout.alignment: Qt.AlignCenter
        }
    }
}
