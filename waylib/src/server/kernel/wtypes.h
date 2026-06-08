// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QtCore/qobjectdefs.h>
#include <QtCore/qmetaobject.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WLR {
public:
    enum Transform {
        Normal = 0,
        R90 = 1,
        R180 = 2,
        R270 = 3,
        Flipped = 4,
        Flipped90 = 5,
        Flipped180 = 6,
        Flipped270 = 7
    };
};

WAYLIB_SERVER_END_NAMESPACE
