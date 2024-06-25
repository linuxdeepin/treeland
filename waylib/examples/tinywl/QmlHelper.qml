// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

pragma Singleton

import QtQuick
import Waylib.Server

Item {
    function printStructureObject(obj) {
        var json = ""
        for (var prop in obj){
            if (!obj.hasOwnProperty(prop)) {
                continue;
            }
            let value = obj[prop]
            try {
                json += `    ${prop}: ${value},\n`
            } catch (err) {
                json += `    ${prop}: unknown,\n`
            }
        }

        return '{\n' + json + '}'
    }
}
