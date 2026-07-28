// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#define static
#include <wlr/render/color.h>
#undef static
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(color_transform)
{
public:
    QW_FUNC_STATIC(color_transform, init_linear_to_icc, qw_color_transform *, const void *data, size_t size)
    QW_FUNC_STATIC(color_transform, init_linear_to_inverse_eotf, qw_color_transform *, enum wlr_color_transfer_function tf)
    QW_FUNC_STATIC(color_transform, init_lut_3x1d, qw_color_transform *, size_t dim, const uint16_t *r, const uint16_t *g, const uint16_t *b)
    QW_FUNC_STATIC(color_transform, init_matrix, qw_color_transform *, const float matrix[9])
    QW_FUNC_STATIC(color_transform, init_pipeline, qw_color_transform *, struct wlr_color_transform **transforms, size_t len)

    QW_FUNC_MEMBER(color_transform, ref, wlr_color_transform *, void)
    QW_FUNC_MEMBER(color_transform, unref, void, void)
};

QW_END_NAMESPACE
