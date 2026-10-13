// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

Item {
    required property SourceOutput output

    width: output.width
    height: menuBar.contentHeight
    x: output.x
    y: output.y

    ToolBar {
        id: menuBar

        width: parent.width

        Row {
            anchors.fill: parent

            ToolButton {
                text: "Quit"
                onClicked: Qt.quit()
            }

            ToolButton {
                text: "Scale"
                onClicked: scaleMenu.popup()

                Menu {
                    id: scaleMenu

                    MenuItem {
                        text: "100%"
                        onClicked: {
                            output.setScale(1)
                        }
                    }

                    MenuItem {
                        text: "125%"
                        onClicked: {
                            output.setScale(1.25)
                        }
                    }

                    MenuItem {
                        text: "150%"
                        onClicked: {
                            output.setScale(1.5)
                        }
                    }

                    MenuItem {
                        text: "175%"
                        onClicked: {
                            output.setScale(1.75)
                        }
                    }

                    MenuItem {
                        text: "200%"
                        onClicked: {
                            output.setScale(2)
                        }
                    }
                }
            }

            ToolButton {
                text: "Rotation"

                onClicked: rotationMenu.popup()

                Menu {
                    id: rotationMenu

                    MenuItem {
                        text: "Normal"
                        onClicked: {
                            output.setTransform(WaylandOutput.Normal)
                        }
                    }

                    MenuItem {
                        text: "R90"
                        onClicked: {
                            output.setTransform(WaylandOutput.R90)
                        }
                    }

                    MenuItem {
                        text: "R270"
                        onClicked: {
                            output.setTransform(WaylandOutput.R270)
                        }
                    }
                }
            }

            ToolButton {
                text: "New Workspace"
                onClicked: WaylandServer.workspace.createModel("Workspace"+Math.random());
            }

            ToolButton {
                text: "Delete Workspace"
                onClicked: {
                    if (WaylandServer.workspace.count > 1)
                        WaylandServer.workspace.removeModel(WaylandServer.workspace.currentIndex);
                }
            }

            ToolButton {
                text: "Prev Workspace"
                onClicked: WaylandServer.workspace.switchToPrev();
            }

            ToolButton {
                text: "Next Workspace"
                onClicked: WaylandServer.workspace.switchToNext();
            }

            Label {
                text: WaylandServer.workspace.currentIndex
                color: "red"
            }

            ToolButton {
                text: "Output"

                onClicked: outputMenu.popup()

                Menu {
                    id: outputMenu

                    MenuItem {
                        text: "Add Output"
                        onClicked: {
                            WaylandServer.addOutput()
                        }
                    }

                    MenuItem {
                        text: (WaylandServer.outputMode === WaylandServer.OutputMode.Copy) ? "Extension Mode" : "Copy Mode"
                        onClicked: {
                            if (WaylandServer.outputMode === WaylandServer.OutputMode.Copy)
                                WaylandServer.outputMode = WaylandServer.OutputMode.Extension
                            else
                                WaylandServer.outputMode = WaylandServer.OutputMode.Copy
                        }
                    }
                }
            }
        }
    }
}
