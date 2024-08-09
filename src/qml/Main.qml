// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Waylib.Server
import TreeLand
import TreeLand.Utils
import TreeLand.Greeter
import TreeLand.Protocols

Item {
    id: root

    Binding {
        target: Helper.seat
        property: "keyboardFocus"
        value: Helper.activatedSurface?.surface || null
    }

    OutputRenderWindow {
        id: renderWindow

        width: Helper.outputLayout.implicitWidth
        height: Helper.outputLayout.implicitHeight

        property OutputDelegate activeOutputDelegate: null

        onOutputViewportInitialized: function (viewport) {
            // Trigger QWOutput::frame signal in order to ensure WOutputHelper::renderable
            // property is true, OutputRenderWindow when will render this output in next frame.
            Helper.enableOutput(viewport.output)
        }

        EventJunkman {
            anchors.fill: parent
            focus: false
        }

        onActiveFocusItemChanged: {
            // Focus does not return to the workspace when a popup is closed from within QML
            if (activeFocusItem === renderWindow.contentItem) {
                loaderContainer.forceActiveFocus()
            }
        }

        Item {
            id: outputLayout
            // register backgroundImage
            property var personalizationMapper: PersonalizationV1.Attached(outputLayout)

            DynamicCreatorComponent {
                id: outputDelegateCreator
                creator: Helper.outputCreator

                OutputDelegate {
                    id: outputDelegate

                    onLastActiveCursorItemChanged: {
                        if (lastActiveCursorItem != null)
                            renderWindow.activeOutputDelegate = outputDelegate
                        else if (renderWindow.activeOutputDelegate === outputDelegate) {
                            for (const output of Helper.outputLayout.outputs) {
                                if (output.lastActiveCursorItem) {
                                    renderWindow.activeOutputDelegate = output
                                    break
                                }
                            }
                        }
                    }

                    Component.onCompleted: {
                        if (!renderWindow.activeOutputDelegate) {
                            renderWindow.activeOutputDelegate = outputDelegate
                        }
                    }

                    Component.onDestruction: {
                        if (renderWindow.activeOutputDelegate === outputDelegate) {
                            for (const output of Helper.outputLayout.outputs) {
                                if (output.lastActiveCursorItem && output !== outputDelegate) {
                                    renderWindow.activeOutputDelegate = output
                                    break
                                }
                            }
                        }
                    }
                }
            }
        }

        FocusScope {
            id: loaderContainer
            anchors.fill: parent
            focus: true
            Loader {
                id: stackLayout
                anchors.fill: parent
                focus: !Helper.lockScreen
                sourceComponent: StackWorkspace {
                    focus: !Helper.lockScreen
                    activeOutputDelegate: renderWindow.activeOutputDelegate
                }
            }

            Loader {
                active: !stackLayout.active
                anchors.fill: parent
                sourceComponent: TiledWorkspace { }
            }
            Loader {
                id: greeter
                active: !TreeLand.testMode
                focus: active
                sourceComponent: Repeater {
                    model: Helper.outputLayout.outputs
                    delegate: Greeter {
                        required property var modelData
                        property CoordMapper outputCoordMapper: this.CoordMapper.helper.get(modelData.output.OutputItem.item)
                        output: modelData.output
                        focus: Helper.lockScreen
                        anchors.fill: outputCoordMapper
                    }
                }
                onLoaded: {
                    Helper.lockScreen = true
                    greeter.forceActiveFocus()
                }
            }
            Component.onCompleted: {
                Helper.lockScreen = !TreeLand.testMode
            }
        }

        Connections {
            target: greeter.active ? GreeterModel : null
            function onAnimationPlayed() {
                Helper.lockScreen = false
            }
            function onAnimationPlayFinished() {
                greeter.active = false
            }
        }

        Connections {
            target: Helper
            function onGreeterVisibleChanged() {
                greeter.active = true
            }
            function onCurrentUserChanged() {
                ShortcutV1.model.user = Helper.currentUser
            }
            function onMetaKeyNotify() {
                if (!Helper.lockScreen) {
                    ShortcutV1.model.handleMetaKey()
                }
            }
        }

        Loader {
            id: logView
            active: TreeLand.debugMode
            x: renderWindow.activeOutputDelegate.x
            y: renderWindow.activeOutputDelegate.y
            width: renderWindow.activeOutputDelegate.width
            height: renderWindow.activeOutputDelegate.height
            property bool autoRefresh: false
            sourceComponent: Rectangle {
                anchors.fill: parent
                color: Qt.rgba(1, 0.9, 0.9, 0.6)
                visible: false

                RenderBufferBlitter {
                    anchors.fill: parent
                    id: blitter
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

                Flickable {
                    id: flickable
                    anchors.fill: parent
                    focus: visible
                    Keys.onUpPressed: scrollBar.decrease()
                    Keys.onDownPressed: scrollBar.increase()
                    ScrollBar.vertical: ScrollBar {
                        id: scrollBar
                        active: true
                    }
                    contentHeight: log.height
                    contentWidth: log.width
                    TextArea {
                        id: log
                        width: logView.width
                        wrapMode: TextEdit.Wrap
                        anchors.margins: 10
                        font.family: "Noto Sans Mono"
                        font.pixelSize: 14
                        Connections {
                            target: LogStream
                            function onBufferChanged() {
                                log.text += LogStream.buffer
                            }
                        }
                    }
                }
            }
        }

        Repeater {
            model: Helper.lockScreen ? undefined : ShortcutV1.model
            Item {
                required property string shortcut
                required property int index
                Shortcut {
                    sequence: shortcut
                    context: Qt.ApplicationShortcut
                    onActivated: {
                        ShortcutV1.model.trigger(index)
                    }
                }
            }
            Component.onCompleted: {
                ShortcutV1.model.user = Helper.currentUser
            }
        }

        Shortcut {
            sequences: ["Meta+S"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.multitaskViewToggled()
            }
        }
        Shortcut {
            sequences: ["Meta+L"]
            autoRepeat: false
            context: Qt.ApplicationShortcut
            onActivated: {
                greeter.active = true
            }
        }
        Shortcut {
            sequences: ["Meta+Right"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.nextWorkspace()
            }
        }
        Shortcut {
            sequences: ["Meta+Left"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.prevWorkspace()
            }
        }
        ListModel {
            id: jumpWSSequences
            ListElement {shortcut: "Meta+1"}
            ListElement {shortcut: "Meta+2"}
            ListElement {shortcut: "Meta+3"}
            ListElement {shortcut: "Meta+4"}
            ListElement {shortcut: "Meta+5"}
            ListElement {shortcut: "Meta+6"}
        }
        Repeater {
            model: jumpWSSequences
            Item {
                required property string shortcut
                required property int index
                Shortcut {
                    sequence: shortcut
                    context: Qt.ApplicationShortcut
                    onActivated: {
                        QmlHelper.shortcutManager.jumpWorkspace(index)
                    }
                }
            }
        }
        Shortcut {
            sequences: ["Meta+Shift+Right"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.moveToNeighborWorkspace(1, null)
            }
        }
        Shortcut {
            sequences: ["Meta+Shift+Left"]
            context: Qt.ApplicationShortcut
            onActivated: {
                QmlHelper.shortcutManager.moveToNeighborWorkspace(-1, null)
            }
        }

        Loader {
            active: TreeLand.debugMode
            sourceComponent: Item {
                Shortcut {
                    sequences: ["Meta+Shift+L"]
                    context: Qt.ApplicationShortcut
                    onActivated: {
                        logView.item.visible = !logView.item.visible
                    }
                }

            }
        }

        Component.onCompleted: {
            QmlHelper.renderWindow = this
        }
    }
}
