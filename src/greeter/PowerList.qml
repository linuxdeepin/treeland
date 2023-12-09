import QtQuick
import TreeLand.Greeter
import org.deepin.dtk 1.0 as D
import QtQuick.Controls

D.Menu {
    id: powers
    modal: true
    padding: 6
    width: 122

    background: D.FloatingPanel {
        anchors.fill: parent
        radius: 12
        blurRadius: 30
        backgroundColor: D.Palette {
            normal: Qt.rgba(238 / 255, 238 / 255, 238 / 255, 0.8)
        }
        insideBorderColor: D.Palette {
            normal: Qt.rgba(1, 1, 1, 0.1)
        }
        outsideBorderColor: D.Palette {
            normal: Qt.rgba(0, 0, 0, 0.06)
        }
        dropShadowColor: D.Palette {
            normal: Qt.rgba(0, 0, 0, 0.2)
        }
    }

    Action {
        enabled: GreeterModel.proxy.canHibernate
        text: qsTr("Hibernate")
        icon.source: "file:///usr/share/icons/Papirus-Dark/symbolic/actions/system-hibernate-symbolic"

        onTriggered: GreeterModel.proxy.hibernate()
    }

    Action {
        enabled: GreeterModel.proxy.canSuspend
        text: qsTr("Suspend")
        icon.source: "file:///usr/share/icons/Papirus-Dark/symbolic/actions/system-suspend-symbolic"

        onTriggered: GreeterModel.proxy.suspend()
    }

    Action {
        enabled: GreeterModel.proxy.canReboot
        text: qsTr("Reboot")
        icon.source: "file:///usr/share/icons/Papirus-Dark/symbolic/actions/system-restart-symbolic"

        onTriggered: GreeterModel.proxy.reboot()
    }

    Action {
        id: powerOff
        enabled: GreeterModel.proxy.canPowerOff
        text: qsTr("Shut Down")
        icon.source: "file:///usr/share/icons/Papirus-Dark/symbolic/actions/system-shutdown-symbolic"

        onTriggered: GreeterModel.proxy.powerOff()
    }

    onAboutToShow: {
        currentIndex = count - 1
    }

    delegate: D.MenuItem {
        id: item
        height: enabled ? 26 : 0
        display: D.IconLabel.IconBesideText
        font.weight: Font.Medium
        font.pixelSize: 13
        font.family: "Source Han Sans CN"
        visible: enabled

        property D.Palette highlightColor: D.Palette {
            normal: D.DTK.makeColor(D.Color.Highlight)
        }

        Keys.onUpPressed: {
            let nextIndex = powers.currentIndex - 1
            for (var index = 0; index < count; index++) {
                if (nextIndex < 0) {
                    nextIndex += powers.count
                }

                let tmp = powers.itemAt(nextIndex)
                if (tmp.visible) {
                    break
                }
                nextIndex--
            }

            powers.currentIndex = nextIndex
        }

        Keys.onDownPressed: {
            let nextIndex = powers.currentIndex + 1
            for (var index = 0; index < count; index++) {
                if (nextIndex >= powers.count) {
                    nextIndex -= powers.count
                }

                let tmp = powers.itemAt(nextIndex)
                if (tmp.enabled) {
                    break
                }
                nextIndex++
            }

            powers.currentIndex = nextIndex
        }

        background: Rectangle {
            id: backgroundColor
            radius: 6
            anchors.fill: parent
            D.BoxShadow {
                anchors.fill: parent
                shadowBlur: 0
                shadowOffsetX: 0
                shadowOffsetY: -1
                cornerRadius: parent.radius
                hollow: true
            }
            color: powers.itemAt(powers.currentIndex)
                   == item ? item.D.ColorSelector.highlightColor : "transparent"
        }
    }
}
