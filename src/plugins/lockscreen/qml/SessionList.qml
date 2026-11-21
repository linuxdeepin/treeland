// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Treeland
import LockScreen

Popup {
    id: popup

    readonly property int sessionItemHeight: 60
    readonly property int listMargin: 10
    readonly property real maxListHeight: 4 * sessionItemHeight + 3 * list.spacing

    modal: false
    width: 220
    height: Math.min(maxListHeight, list.contentHeight) + topPadding + bottomPadding + listMargin * 2
    objectName: "LoginScreenSessionListPopup"

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: popup.listMargin
        spacing: 1
        clip: true
        model: SessionModel

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            parent: popup.contentItem
            policy: ScrollBar.AlwaysOn

            anchors {
                top: list.top
                bottom: list.bottom
                right: parent.right
                rightMargin: 2 - popup.rightPadding - verticalScrollBar.rightPadding
            }
        }

        delegate: Item {
            required property string name
            required property int index
            width: parent.width
            height: popup.sessionItemHeight
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                onEntered: {
                    background.color = Qt.rgba(0, 129/255, 255/255, 0.9)
                }
                onExited: {
                    background.color = Qt.rgba(0, 0, 0, 0.05)
                }
                onClicked: (mouse) => {
                    mouse.accepted = false
                    SessionModel.currentIndex = index
                    popup.close()
                }
            }

            Rectangle {
                id: background
                width: parent.width
                height: parent.height
                color: Qt.rgba(0, 0, 0, 0.05)
                radius: 15
            }
            RowLayout {
                spacing: 5
                anchors.fill: parent
                Text {
                    text: name
                    Layout.leftMargin: 10
                    Layout.fillWidth: true
                }
                Image {
                    // logined
                }
            }
        }
    }
    onAboutToShow: {
        // Fetch the session list on demand instead of watching the session dirs.
        SessionModel.reload()
        list.currentIndex = SessionModel.currentIndex
    }
}
