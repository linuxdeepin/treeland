// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick

Item {
    property alias model: dockListView.model

    ListView {
        id: dockListView
        height: Math.min(parent.height, contentHeight)
        spacing: 8
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            right: parent.right
        }

        model: ListModel {
            id: dockModel
            function removeSurface(surface) {
                for (var i = 0; i < dockModel.count; i++) {
                    if (dockModel.get(i).source === surface) {
                        dockModel.remove(i);
                        break;
                    }
                }
            }
        }

        delegate: ShaderEffectSource {
            id: dockitem
            width: 100; height: 100
            sourceItem: source
            smooth: true

            MouseArea {
                anchors.fill: parent;
                onClicked: {
                    dockitem.sourceItem.cancelMinimize();
                }
            }
        }
    }
}
