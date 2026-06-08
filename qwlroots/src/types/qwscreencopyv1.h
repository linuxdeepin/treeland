// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_screencopy_v1.h>
}

QW_BEGIN_NAMESPACE
class QW_CLASS_OBJECT(screencopy_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(screencopy_manager_v1, create, qw_screencopy_manager_v1 *, wl_display *display)
};
QW_END_NAMESPACE
