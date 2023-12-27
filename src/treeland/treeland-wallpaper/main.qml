// Copyright (C) 2023 pengwenhao <pengwenhao@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    visible: true
    width: 720
    height: 450
    title: qsTr("Wallpaper Settings")

    Wallpaper {
        anchors.fill: parent
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.topMargin: 16
        anchors.bottomMargin: 16
    }
}
