// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#define static
#include <wlr/types/wlr_virtual_pointer_v1.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(virtual_pointer_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_virtual_pointer, wlr_virtual_pointer_v1_new_pointer_event*)

public:
    QW_FUNC_STATIC(virtual_pointer_manager_v1, create, qw_virtual_pointer_manager_v1*, wl_display *display)
};

/** TODO: wlr_virtual_pointer_v1 not wrapped now, wrapping it should cause a complex inheritance hierarchy.
  * Users should just care about wlr_virtual_pointer_v1.pointer as a wlr_pointer.
  */

QW_END_NAMESPACE
