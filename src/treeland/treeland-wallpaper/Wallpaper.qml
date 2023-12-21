import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15
import QtQuick.Dialogs
import org.deepin.dtk 1.0
import Wallpaper

ColumnLayout {
    id: root
    anchors.fill: parent
    spacing: 16

    PersonalizationManager {
        id: personalization

        onWallpaperChanged: {
            backgroundImage.source = path;
        }
    }

    Text {
        id: title
        Layout.alignment: Qt.AlignTop
        width: parent.width
        height: 32
        verticalAlignment: Text.AlignVCenter
        text: qsTr("Wallpaper")
    }

    Item {
        id: info
        Layout.alignment: Qt.AlignTop
        implicitHeight: parent.height - title.height - parent.spacing
        implicitWidth: parent.width

        RowLayout{
            spacing: 8
            implicitWidth: parent.width

            Rectangle {
                id: background
                height: 135
                width: 240
                radius: 8

                Image {
                    id: backgroundImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize: Qt.size(background.width, background.height)
                }
            }

            Item {
                id: infoItem
                Layout.preferredHeight: background.height
                Layout.preferredWidth: info.width - root.width - parent.spacing

                Column {
                    Layout.preferredWidth: parent.width
                    spacing: 10

                    ColumnLayout {
                        id: information
                        implicitWidth: parent.width
                        spacing: 1
                        PropertyItemDelegate {
                            Layout.fillWidth: true
                            title: qsTr("Current Wallpaepr")

                            corners: RoundRectangle.TopCorner
                            action: Text {
                                text: "wave of the blue"
                            }
                        }

                        PropertyItemDelegate {
                            Layout.fillWidth: true
                            title: qsTr("Wallpaper Display Method")

                            corners: RoundRectangle.BottomCorner
                            action: ComboBox {
                                ColorSelector.family: Palette.CommonColor
                                Layout.fillWidth: true
                                model: ListModel {
                                    ListElement { text: "Stretch"}
                                    ListElement { text: "Preserve Aspect Fit"}
                                    ListElement { text: "entry_voice"}
                                }
                                textRole: "text"
                            }
                        }
                    }

                    FileDialog {
                        id: fileDialog
                        title: "Please choose a file"

                        onAccepted: {
                            personalization.setWallpaper(fileDialog.selectedFile);
                        }
                    }

                    RowLayout {
                        implicitWidth: parent.width
                        Layout.alignment: Qt.AlignBottom | Qt.AlignHCenter
                        spacing: 8

                        Button {
                            id: add_wallpaper
                            property var wallpaperPath : fileDialog.fileUrls
                            text: qsTr("Add Wallpaper")
                            Layout.preferredWidth: (infoItem.width - parent.spacing) / 2
                            Layout.alignment: Qt.AlignLeft

                            onClicked: fileDialog.open()
                        }

                        Button {
                            id: add_directory
                            text: "Add Directory"
                            Layout.preferredWidth: (infoItem.width - parent.spacing) / 2
                            Layout.alignment: Qt.AlignRight
                        }
                    }
                }
            }
        }
    }
}
