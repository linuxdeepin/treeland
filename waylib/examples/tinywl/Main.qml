// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Waylib.Server
import Tinywl

Item {
    id :root

    Binding {
        target: Helper.seat
        property: "keyboardFocus"
        value: Helper.getFocusSurfaceFrom(renderWindow.activeFocusItem)
    }

    OutputRenderWindow {
        id: renderWindow

        width: Helper.outputLayout.implicitWidth
        height: Helper.outputLayout.implicitHeight

        onOutputViewportInitialized: function (viewport) {
            // Trigger QWOutput::frame signal in order to ensure WOutputHelper::renderable
            // property is true, OutputRenderWindow when will render this output in next frame.
            Helper.enableOutput(viewport.output)
        }

        EventJunkman {
            anchors.fill: parent
        }

        Item {
            DynamicCreatorComponent {
                creator: Helper.outputCreator

                OutputDelegate {
                    property real topMargin: topbar.height
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent

            TabBar {
                id: topbar

                Layout.fillWidth: true

                TabButton {
                    text: qsTr("Stack Layout")
                    onClicked: {
                        Helper.xdgDecorationManager.preferredMode = XdgDecorationManager.Client
                    }
                }
                TabButton {
                    text: qsTr("Tiled Layout")
                    onClicked: {
                        Helper.xdgDecorationManager.preferredMode = XdgDecorationManager.Server
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackWorkspace {
                    visible: topbar.currentIndex === 0
                    anchors.fill: parent
                }

                TiledWorkspace {
                    visible: topbar.currentIndex === 1
                    anchors.fill: parent
                }
            }
        }
    }
}
