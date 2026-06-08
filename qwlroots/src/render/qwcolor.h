// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/render/color.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(color_transform)
{
public:
    QW_FUNC_STATIC(color_transform, init_linear_to_icc, qw_color_transform *, const void *data, size_t size)
    QW_FUNC_STATIC(color_transform, init_srgb, wlr_color_transform *, void)

    QW_FUNC_MEMBER(color_transform, ref, wlr_color_transform *, void)
    QW_FUNC_MEMBER(color_transform, unref, void, void)
};

QW_END_NAMESPACE
