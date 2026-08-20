// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import Waylib.Server
import Treeland

Item {
    required property PrimaryOutput output

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
                onClicked: Helper.workspace.createModel("Workspace"+Math.random());
            }

            ToolButton {
                text: "Delete Workspace"
                onClicked: {
                    if (Helper.workspace.count > 1)
                        Helper.workspace.removeModel(Helper.workspace.currentIndex);
                }
            }

            ToolButton {
                text: "Prev Workspace"
                onClicked: Helper.workspace.switchToPrev();
            }

            ToolButton {
                text: "Next Workspace"
                onClicked: Helper.workspace.switchToNext();
            }

            Label {
                text: Helper.workspace.currentIndex
                color: "red"
            }

            ToolButton {
                text: "Damage"
                onClicked: damageMenu.popup()

                Menu {
                    id: damageMenu

                    MenuItem {
                        text: "Off"
                        onClicked: Helper.setDamageDebugMode("none")
                    }
                    MenuItem {
                        text: "Highlight"
                        onClicked: Helper.setDamageDebugMode("highlight")
                    }
                    MenuItem {
                        text: "Log"
                        onClicked: Helper.setDamageDebugMode("log")
                    }
                    MenuItem {
                        text: "Rerender"
                        onClicked: Helper.setDamageDebugMode("rerender")
                    }
                }
            }

            ToolButton {
                text: "Output"

                onClicked: outputMenu.popup()

                Menu {
                    id: outputMenu

                    MenuItem {
                        text: "Add Output"
                        onClicked: {
                            Helper.addOutput()
                        }
                    }

                    MenuItem {
                        text: (Helper.outputMode === Helper.OutputMode.Copy) ? "Extension Mode" : "Copy Mode"
                        onClicked: {
                            if (Helper.outputMode === Helper.OutputMode.Copy)
                                Helper.outputMode = Helper.OutputMode.Extension
                            else
                                Helper.outputMode = Helper.OutputMode.Copy
                        }
                    }
                }
            }
        }
    }
}
