// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_export_dmabuf_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(export_dmabuf_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(export_dmabuf_manager_v1, create, qw_export_dmabuf_manager_v1 *, wl_display *display)
};

QW_END_NAMESPACE
