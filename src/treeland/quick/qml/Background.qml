import QtQuick
import TreeLand.Protocols

Item {
    id: root

    Image {
        id: background
        fillMode: Image.PreserveAspectCrop
        source: personalizationManager.currentWallpaper + "?" + new Date().getTime()
        anchors.fill: parent
        asynchronous: true

        onStatusChanged: {
            if (background.status == Image.Loading)
              // transitionAnimation.opacity = 100;
            // console.log("change opacity to 100");
            {
            }
        }
    }

    ShaderEffectSource {
        id: transitionAnimation
        anchors.fill: parent
        sourceItem: background
        hideSource: false
        live: false

        states: [
            State {
                name: "loading"
                when: background.status == Image.Loading
                PropertyChanges {
                    target: transitionAnimation
                    visible: true
                    opacity: 1
                }
            },
            State {
                name: "ready"
                when: background.status == Image.Ready
                PropertyChanges {
                    target: transitionAnimation
                    opacity: 0
                }
            },
            State {
                name: "finished"
                when: transitionAnimation.opacity == 0
                PropertyChanges {
                    target: transitionAnimation
                    visible: false
                }
            }
        ]

        onOpacityChanged: {
          console.log("-- ", opacity)
        }

        transitions: [
            Transition {
                to: "loading"
                PropertyAnimation {
                    properties: "opacity"
                    duration: 300
                }
            },
            Transition {
                to: "ready"
                PropertyAnimation {
                    properties: "opacity"
                    duration: 300
                }
            }
        ]
    }
}
