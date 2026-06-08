// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wlr_abc.h"
#include <qwobject.h>

class QW_CLASS_OBJECT(abc)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(set_name, char*)

public:
    QW_FUNC_STATIC(abc, create, qw_abc*, wl_display*)

    QW_FUNC_MEMBER(abc, sum, int)
    QW_FUNC_MEMBER(abc, set_name, void, char *)
};
