import QtQuick 2.15
import capture

CanvasWindow {
    width: 600
    height: 400
    visible: true
    flags: Qt.FramelessWindowHint
    color: "transparent"
    Image {
        id: watermark
        source: "qrc:/watermark.png"
        sourceSize: Qt.size(100, 100)
        fillMode: Image.Tile
        anchors.fill: parent
    }
}
