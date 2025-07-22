// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/render/drm_format_set.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(drm_format)
{
public:
    QW_FUNC_MEMBER(drm_format, finish, void)
};

class QW_CLASS_REINTERPRET_CAST(drm_format_set)
{
public:
    QW_FUNC_MEMBER(drm_format_set, finish, void)
    QW_FUNC_MEMBER(drm_format_set, get, const drm_format_set *, uint32_t format)
#if WLR_VERSION_MINOR >= 19
    QW_FUNC_MEMBER(drm_format_set, remove, bool, uint32_t format, uint64_t modifier)
#endif
    QW_FUNC_MEMBER(drm_format_set, has, bool, uint32_t format, uint64_t modifier)
    QW_FUNC_MEMBER(drm_format_set, add, bool, uint32_t format, uint64_t modifier)

    QW_FUNC_MEMBER(drm_format_set, intersect, bool, const drm_format_set *a, const drm_format_set *b)
    QW_FUNC_STATIC(drm_format_set, union, bool, drm_format_set *dst, 
        const drm_format_set *a, const drm_format_set *b)
};

QW_END_NAMESPACE
