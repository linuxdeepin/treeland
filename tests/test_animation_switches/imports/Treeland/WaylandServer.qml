// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton
import QtQuick

QtObject {
    readonly property QtObject config: QtObject {
        readonly property color activeColor: "blue"
    }
    property QtObject animationConfig
}
