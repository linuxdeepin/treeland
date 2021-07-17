import QtQuick 2.0
import QtQuick.Window 2.15
import QtQuick.Controls 2.0
import org.zjide.waylib 1.0

Item {
    id: root
    width: Window.width
    height: Window.height

    property var output

    Image {
        id: background
        sourceSize: Qt.size(root.width, root.height)
        source: "file:///usr/share/backgrounds/example.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Item {
        id: content
        objectName: "WindowLayer"
        anchors {
            fill: parent
            rightMargin: 150
        }
    }

    Column {
        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: 10
        }

        spacing: 10

        Button {
            text: "1X"
            onClicked: {
                output.scale = 1
            }
        }

        Button {
            text: "1.5X"
            onClicked: {
                output.scale = 1.5
            }
        }

        Button {
            text: "Normal"
            onClicked: {
                output.orientation = Output.Normal
            }
        }

        Button {
            text: "R90"
            onClicked: {
                output.orientation = Output.R90
            }
        }

        Button {
            text: "R270"
            onClicked: {
                output.orientation = Output.R270
            }
        }

        Button {
            text: "Quit"
            onClicked: {
                Qt.quit()
            }
        }
    }
}
