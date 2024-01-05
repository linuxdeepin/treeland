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
    property alias descriptionVisiable: description.visible

    implicitWidth: parent.width
    implicitHeight: head.height + gridView.contentHeight +  10

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

                    MouseArea {
                        anchors.fill: parent

                        onClicked: {
                            modelData.showAll = !modelData.showAll
                        }
                    }
                }
            }
        }

        GridView {
            id: gridView
            implicitWidth: parent.width
            implicitHeight: parent.height
            clip: true
            interactive: false
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
                    sourceSize: Qt.size(width, height)
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
                    hoverEnabled: true
                    onClicked: {
                        personalization.setWallpaper(imageSource, group, index)
                    }

                    onEntered: {
                        if (gridView.currentIndex !== index && group === "Local")
                            removeButton.visible = true
                    }

                    onExited: {
                        removeButton.visible = false;
                    }
                }

                FloatingButton {
                    id: removeButton
                    x: itemImage.width - removeButton.width / 2
                    y: 0
                    visible: false

                    icon {
                        width: 8
                        height: 8
                        name: "clear"
                    }

                    MouseArea {
                        id: removeButtonArea
                        anchors.fill: parent

                        onClicked: {
                            personalization.removeWallpaper(imageSource, group, index)
                        }
                    }
                }
            }
        }
    }
}
