// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_shm.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(shm)
{
public:
    QW_FUNC_STATIC(shm, create, qw_shm *, wl_display *display, uint32_t version, const uint32_t *formats, size_t formats_len)
    QW_FUNC_STATIC(shm, create_with_renderer, qw_shm *, wl_display *display, uint32_t version, wlr_renderer *renderer)
};

QW_END_NAMESPACE
