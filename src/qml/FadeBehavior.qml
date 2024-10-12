// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQml

Behavior {
    id: root

    property QtObject fadeTarget: targetProperty.object
    property string fadeProperty: "opacity"
    property var fadeProperties: [fadeProperty]
    property int exitValue: 0
    property int enterValue: exitValue
    property int fadeDuration: 300
    property string easingType: "Quad"
    property bool delayWhile: false
    property bool sequential: false

    onDelayWhileChanged: {
        if (!delayWhile)
            sequentialAnimation.startExitAnimation();
    }

    readonly property Component shaderEffectSourceWrapperComponent: Item {
        property alias shaderEffectSource: shaderEffectSource
        property alias sourceItem: shaderEffectSource.sourceItem
        parent: sourceItem.parent
        x: sourceItem.x
        y: sourceItem.y
        ShaderEffectSource {
            id: shaderEffectSource
            transformOrigin: sourceItem.transformOrigin
            hideSource: true
            live: false
            width: sourceItem.width
            height: sourceItem.height
        }
    }

    readonly property Component defaultExitAnimation: NumberAnimation {
        properties: root.fadeProperties.join(',')
        duration: root.fadeDuration
        to: root.exitValue
        easing.type: root.easingType === "Linear" ? Easing.Linear : Easing["In"+root.easingType]
    }
    property Component exitAnimation: defaultExitAnimation

    readonly property Component defaultEnterAnimation: NumberAnimation {
        properties: root.fadeProperties.join(',')
        duration: root.fadeDuration
        from: root.enterValue
        to: root.fadeTarget[root.fadeProperties[0]]
        easing.type: root.easingType === "Linear" ? Easing.Linear : Easing["Out"+root.easingType]
    }
    property Component enterAnimation: NumberAnimation {
       properties: root.fadeProperties.join(',')
       duration: root.fadeDuration
       from: root.enterValue
       to: root.fadeTarget[root.fadeProperties[0]]
       easing.type: root.easingType === "Linear" ? Easing.Linear : Easing["Out"+root.easingType]
    }

    SequentialAnimation {
        id: sequentialAnimation
        signal startEnterAnimation()
        signal startExitAnimation()
        ScriptAction {
            script: {
                const exitItem = shaderEffectSourceWrapperComponent.createObject(null, { sourceItem: root.fadeTarget });
                const exitShaderEffectSource = exitItem.shaderEffectSource;
                if (exitAnimation === root.defaultExitAnimation)
                    root.fadeProperties.forEach(p => exitShaderEffectSource[p] = root.fadeTarget[p]);
                exitShaderEffectSource.width = root.fadeTarget.width;
                exitShaderEffectSource.height = root.fadeTarget.height;
                const exitAnimationInstance = exitAnimation.createObject(root, { target: exitItem.shaderEffectSource });

                sequentialAnimation.startExitAnimation.connect(exitAnimationInstance.start);
                if (root.sequential)
                    exitAnimationInstance.finished.connect(sequentialAnimation.startEnterAnimation);
                else
                    exitAnimationInstance.started.connect(sequentialAnimation.startEnterAnimation);

                exitAnimationInstance.finished.connect(() => {
                    exitItem.destroy();
                    exitAnimationInstance.destroy();
                });
            }
        }
        PropertyAction {}
        ScriptAction {
            script: {
                const enterItem = shaderEffectSourceWrapperComponent.createObject(null, { sourceItem: root.fadeTarget });
                const enterShaderEffectSource = enterItem.shaderEffectSource;
                if (enterAnimation === root.defaultEnterAnimation)
                    root.fadeProperties.forEach(p => enterShaderEffectSource[p] = root.enterValue);
                enterShaderEffectSource.live = true;
                const enterAnimationInstance = enterAnimation.createObject(root, { target: enterItem.shaderEffectSource });

                sequentialAnimation.startEnterAnimation.connect(enterAnimationInstance.start);

                enterAnimationInstance.finished.connect(() => {
                    enterItem.destroy();
                    enterAnimationInstance.destroy();
                });

                if (!root.delayWhile)
                    sequentialAnimation.startExitAnimation();
            }
        }
    }
}
