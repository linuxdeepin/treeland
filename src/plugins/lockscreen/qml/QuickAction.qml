// Copyright (C) 2023 justforlxz <justforlxz@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Treeland

Column {
    id: root

    TimeDateWidget {
        id: timedate
        currentLocale :{
            let user = UserModel.get(UserModel.currentUserName)
            return user.locale
        }
        width: 400
        height: 157
        background: RoundBlur {
            radius: 8
        }
    }
}

