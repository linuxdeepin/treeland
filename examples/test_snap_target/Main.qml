import QtQuick

// One instance per output. `snapController` is injected as a per-window
// context property so that each window tracks its own output origin while
// sharing the process-wide snap session.
Window {
    id: root
    visible: true
    flags: Qt.FramelessWindowHint
    color: "transparent"
    width: snapController.screenWidth
    height: snapController.screenHeight

    // Frozen snapshot of this output, captured before any mask window was
    // mapped. Only present when started with --background.
    Image {
        id: backgroundImage
        anchors.fill: parent
        fillMode: Image.Stretch
        source: snapController.backgroundSource
        visible: snapController.backgroundEnabled
    }

    Rectangle {
        id: snapHighlight
        color: Qt.rgba(0.2, 0.6, 1.0, 0.1)
        border.color: "#3399FF"
        border.width: 3
        visible: snapController.snapVisible && !snapController.confirmed
        x: snapController.snapX
        y: snapController.snapY
        width: snapController.snapWidth
        height: snapController.snapHeight
        radius: 4
    }

    // Frozen selection highlight after confirmation
    Rectangle {
        id: frozenSelection
        color: Qt.rgba(0.2, 0.6, 1.0, 0.15)
        border.color: "#3399FF"
        border.width: 4
        visible: snapController.confirmed
        x: snapController.snapX
        y: snapController.snapY
        width: snapController.snapWidth
        height: snapController.snapHeight
        radius: 4
    }

    // Full-screen click area — only active before confirmation
    MouseArea {
        id: clickCatcher
        anchors.fill: parent
        enabled: !snapController.confirmed
        cursorShape: Qt.CrossCursor
        onClicked: snapController.confirmSelection()
    }

    // Demo toolbar (non-functional) shown after confirmation
    Rectangle {
        id: toolbar
        z: 9
        visible: snapController.confirmed
        color: "#2B2B2B"
        border.color: "#555555"
        border.width: 1
        radius: 6
        height: 48
        width: 260

        // Center toolbar above the frozen selection, clamped to this output
        x: snapController.confirmed
           ? Math.max(8, Math.min(root.width - width - 8,
                                  snapController.snapX + snapController.snapWidth / 2 - width / 2))
           : 0
        y: snapController.confirmed
           ? (snapController.snapY - height - 8 >= 8
              ? snapController.snapY - height - 8
              : Math.min(root.height - height - 8,
                         snapController.snapY + snapController.snapHeight + 8))
           : 0

        Row {
            anchors.centerIn: parent
            spacing: 10

            Repeater {
                model: [
                    { icon: "✎", label: "Annotate" },
                    { icon: "⌨", label: "Text" },
                    { icon: "⏲", label: "Delay" },
                    { icon: "💾", label: "Save" }
                ]

                Rectangle {
                    width: 52
                    height: 36
                    color: "transparent"
                    radius: 4

                    Text {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 2
                        text: modelData.icon
                        font.pixelSize: 18
                        color: "#FFFFFF"
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottomMargin: 2
                        text: modelData.label
                        font.pixelSize: 9
                        color: "#AAAAAA"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // Demo only - no actual functionality
                        }
                    }
                }
            }
        }

        // Close button on the toolbar's right edge
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            height: 24
            radius: 12
            color: "transparent"

            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: 18
                color: "#FFFFFF"
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: snapController.quit()
            }
        }
    }

    // Debug overlay - one per output
    Text {
        z: 10
        x: 10
        y: 10
        text: "confirmed=" + snapController.confirmed
              + " vis=" + snapController.snapVisible
              + " region=(" + snapController.snapX + "," + snapController.snapY
              + " " + snapController.snapWidth + "x" + snapController.snapHeight + ")"
              + " proc=" + snapController.processName
              + " output=(" + snapController.screenX + "," + snapController.screenY
              + " " + root.width + "x" + root.height + ")"
        color: "red"
        font.pixelSize: 14
    }
}
