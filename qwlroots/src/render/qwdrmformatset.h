// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <drm_fourcc.h>
#include <wlr/render/drm_format_set.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(drm_format)
{
public:
    QW_FUNC_MEMBER(drm_format, finish, void)

    QW_ALWAYS_INLINE bool has_modifier(uint64_t modifier) const
    {
        return has_modifier(handle(), modifier);
    }

    QW_ALWAYS_INLINE static bool has_modifier(const wlr_drm_format *format, uint64_t modifier)
    {
        if (!format)
            return false;

        for (size_t i = 0; i < format->len; ++i) {
            if (format->modifiers[i] == modifier)
                return true;
        }

        return false;
    }
};

class QW_CLASS_REINTERPRET_CAST(drm_format_set)
{
public:
    QW_FUNC_MEMBER(drm_format_set, finish, void)
    QW_FUNC_MEMBER(drm_format_set, get, const wlr_drm_format *, uint32_t format)
#if WLR_VERSION_MINOR >= 19
    QW_FUNC_MEMBER(drm_format_set, remove, bool, uint32_t format, uint64_t modifier)
#endif
    QW_FUNC_MEMBER(drm_format_set, has, bool, uint32_t format, uint64_t modifier)
    QW_FUNC_MEMBER(drm_format_set, add, bool, uint32_t format, uint64_t modifier)

    QW_FUNC_MEMBER(drm_format_set, intersect, bool, const wlr_drm_format_set *a, const wlr_drm_format_set *b)

    QW_ALWAYS_INLINE bool has_modifier(uint64_t modifier) const
    {
        return has_modifier(handle(), modifier);
    }

    QW_ALWAYS_INLINE bool has_implicit_modifier() const
    {
        return has_implicit_modifier(handle());
    }

    QW_ALWAYS_INLINE static bool unite(wlr_drm_format_set *dst,
                                       const wlr_drm_format_set *a,
                                       const wlr_drm_format_set *b)
    {
        return wlr_drm_format_set_union(dst, a, b);
    }

    QW_ALWAYS_INLINE static bool has_modifier(const wlr_drm_format_set *set, uint64_t modifier)
    {
        if (!set)
            return false;

        for (size_t i = 0; i < set->len; ++i) {
            if (qw_drm_format::has_modifier(&set->formats[i], modifier))
                return true;
        }

        return false;
    }

    QW_ALWAYS_INLINE static bool has_implicit_modifier(const wlr_drm_format_set *set)
    {
        return has_modifier(set, DRM_FORMAT_MOD_INVALID);
    }
};

QW_END_NAMESPACE
