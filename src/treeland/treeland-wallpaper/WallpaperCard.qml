// Copyright (C) 2023 pengwenhao <pengwenhao@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick 2.0
import QtQuick.Layouts 1.11
import org.deepin.dtk 1.0

Item {
    property string group
    property string directory

    property alias modelData: gridView.model
    property alias titleText: title.text
    property alias descriptionText: description.text

    implicitWidth: parent.width
    implicitHeight: 240

    ColumnLayout {
        spacing: 10
        anchors.fill: parent

        Item {
            id: head
            implicitWidth: parent.width
            implicitHeight: 32

            RowLayout {
                anchors.fill: parent

                Text {
                    id: title
                    Layout.alignment: Qt.AlignLeft
                    width: parent.width / 2
                    height: 32
                }

                Text {
                    id: description
                    Layout.alignment: Qt.AlignRight
                    width: parent.width / 2
                    height: 32
                }
            }
        }

        GridView {
            id: gridView
            implicitWidth: parent.width
            implicitHeight: parent.height - head.height - parent.spacing
            cellWidth: 140
            cellHeight: 105
            currentIndex: model.currentIndex

            delegate: Item {
                width: gridView.cellWidth
                height: gridView.cellHeight

                Image {
                    id: itemImage
                    anchors.margins: 8
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize: Qt.size(width - 10, height - 10)
                    source: imageSource
                }

                Rectangle {
                    id: maskRect
                    anchors.fill: parent
                    anchors.margins: 5
                    anchors.centerIn: parent.Center
                    border.width: 5
                    border.color: (gridView.currentIndex === index && personalization.currentGroup === group)
                                  ? "blue" : "transparent"
                    radius: 10
                }

                OpacityMask {
                    anchors.margins: 8
                    anchors.fill: parent
                    source: itemImage
                    maskSource: maskRect
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        personalization.setWallpaper(imageSource, group, index)
                    }
                }
            }
        }
    }
}
