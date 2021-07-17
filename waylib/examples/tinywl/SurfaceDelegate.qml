import QtQuick 2.0
import org.zjide.waylib 1.0

Item {
    property alias surface: window.surface

    SurfaceItem {
        id: window
        anchors.fill: parent
    }
}
