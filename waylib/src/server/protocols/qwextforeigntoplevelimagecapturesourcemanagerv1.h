// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include "ext_foreign_toplevel_image_capture_source_manager_v1.h"
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(ext_foreign_toplevel_image_capture_source_manager_v1)
{
    QW_OBJECT
    Q_OBJECT
    QW_SIGNAL(new_request, wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request*)
public:
    QW_FUNC_STATIC(ext_foreign_toplevel_image_capture_source_manager_v1, create, qw_ext_foreign_toplevel_image_capture_source_manager_v1 *, wl_display *display, uint32_t version)
    QW_FUNC_STATIC(ext_foreign_toplevel_image_capture_source_manager_v1, request_accept, bool, wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request, wlr_ext_image_capture_source_v1 *source)
};

QW_END_NAMESPACE
