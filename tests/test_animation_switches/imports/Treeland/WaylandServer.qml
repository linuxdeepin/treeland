// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton
import QtQuick

QtObject {
    property int effectLevel: 3
    property QtObject config

    function isEffectEnabled(currentLevel, minLevel) {
        return minLevel > 0 && currentLevel >= minLevel
    }
}
