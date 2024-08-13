// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Dialogs
import org.deepin.dtk 1.0 as D
import TreeLand
import TreeLand.Utils
import Waylib.Server

Column {
    id: root

    TimeDateWidget {
        id: timedate
        currentLocale :{
            let user = GreeterModel.userModel.get(GreeterModel.currentUser)
            return user.locale
        }
        width: 400
        height: 156
        background: Rectangle {
            radius: 8
            color: "white"
            opacity: 0.1
        }
        RenderBufferBlitter {
            id: blitter
            parent: timedate
            z: parent.z - 1
            anchors.fill: parent
            MultiEffect {
                id: blur
                anchors.fill: parent
                source: blitter.content
                autoPaddingEnabled: false
                blurEnabled: true
                blur: 1.0
                blurMax: 64
                saturation: 0.2
            }
        }
    }
}

