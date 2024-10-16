import QtQuick
import Treeland
import QtQuick.Effects


Item {
    id: root
    required property QtObject output
    required property WorkspaceModel workspace
    required property int workspaceListPadding
    required property Component delegate

    readonly property real delegateCornerRadius: (ros.rows >= 1 && ros.rows <= 3) ? ros.cornerRadiusList[ros.rows - 1] : ros.cornerRadiusList[2]

    QtObject {
        id: ros // readonly state
        property list<real> cornerRadiusList: [18,12,8] // Should get from system preference
        readonly property int rows: surfaceModel.rows
    }

    MultitaskviewSurfaceModel {
        id: surfaceModel
        surfaceListModel: root.workspace
        layoutArea: root.mapToItem(output.outputItem, Qt.rect(surfaceGridView.x, surfaceGridView.y, surfaceGridView.width, surfaceGridView.height))
    }

    Flickable {
        id: surfaceGridView
        // Do not use anchors here, geometry should be stable as soon as the item is created
        y: workspaceListPadding
        width: parent.width
        height: parent.height - workspaceListPadding
        visible: surfaceModel.modelReady
        Repeater {
            model: surfaceModel
            Loader {
                id: surfaceLoader
                visible: true
                sourceComponent: delegate
                required property int index
                required property SurfaceWrapper wrapper
                required property rect geometry
                required property bool padding
                state: root.state
                property bool needPadding
                property real paddingOpacity
                readonly property rect initialGeometry: surfaceGridView.mapFromItem(output.outputItem, wrapper.geometry)
                x: geometry.x
                y: geometry.y
                z: index
                width: geometry.width
                height: geometry.height
                states: [
                    State {
                        name: "initial"
                        PropertyChanges {
                            surfaceLoader {
                                x: initialGeometry.x
                                y: initialGeometry.y
                                width: initialGeometry.width
                                height: initialGeometry.height
                                paddingOpacity: 0
                                needPadding: false
                            }
                        }
                    },
                    State {
                        name: "partial"
                        PropertyChanges {
                            surfaceLoader {
                                x: (geometry.x - initialGeometry.x) * partialGestureFactor + initialGeometry.x
                                y: (geometry.y - initialGeometry.y) * partialGestureFactor + initialGeometry.y
                                width: (geometry.width - initialGeometry.width) * partialGestureFactor + initialGeometry.width
                                height: (geometry.height - initialGeometry.height) * partialGestureFactor + initialGeometry.height
                                paddingOpacity: TreelandConfig.multitaskviewPaddingOpacity * partialGestureFactor
                                needPadding: true
                            }
                        }
                    },
                    State {
                        name: "taskview"
                        PropertyChanges {
                            surfaceLoader {
                                x: geometry.x
                                y: geometry.y
                                width: geometry.width
                                height: geometry.height
                                needPadding: padding
                                paddingOpacity: TreelandConfig.multitaskviewPaddingOpacity
                            }
                        }
                    }
                ]
                transitions: [
                    Transition {
                        to: "initial, taskview"
                        ParallelAnimation {
                            NumberAnimation {
                                properties: "x, y, width, height, paddingOpacity"
                                duration: TreelandConfig.multitaskviewAnimationDuration
                                easing.type: TreelandConfig.multitaskviewEasingCurveType
                            }
                            PropertyAction {
                                target: surfaceLoader
                                property: "needPadding"
                            }
                        }
                    }
                ]
            }
        }
    }

    Component.onCompleted: {
        surfaceModel.calcLayout()
    }
}
