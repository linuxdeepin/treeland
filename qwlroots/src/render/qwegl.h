// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/render/egl.h>
#include <EGL/egl.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(egl)
{
public:
    QW_FUNC_MEMBER(egl, create_with_context, wlr_egl *, EGLContext context)
    QW_FUNC_MEMBER(egl, get_display, EGLDisplay)
    QW_FUNC_MEMBER(egl, get_context, EGLContext)
};

QW_END_NAMESPACE
