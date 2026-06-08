// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/types/wlr_damage_ring.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_BOX(damage_ring)
{
public:
#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(damage_ring, add, bool, const pixman_region32_t *damage)
#else
    QW_FUNC_MEMBER(damage_ring, add, void, const pixman_region32_t *damage)
#endif
    QW_FUNC_MEMBER(damage_ring, add_box, bool, const wlr_box *box)
    QW_FUNC_MEMBER(damage_ring, add_whole, void)
#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(damage_ring, rotate, void)
#else
    QW_FUNC_MEMBER(damage_ring, rotate_buffer, void, wlr_buffer *buffer, pixman_region32_t *damage)
#endif

#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(damage_ring, set_bounds, void, int32_t width, int32_t height)
#endif

private:
    friend qw_class_box;
    QW_FUNC_MEMBER(damage_ring, init, void)
    QW_FUNC_MEMBER(damage_ring, finish, void)
};

QW_END_NAMESPACE
