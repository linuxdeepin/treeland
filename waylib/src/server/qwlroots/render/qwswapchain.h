// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#define slots slots_c
#include <wlr/render/swapchain.h>
#undef slots
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(swapchain)
{
public:
    QW_FUNC_STATIC(swapchain, create, qw_swapchain *, wlr_allocator *alloc, int width, int height, const wlr_drm_format *format)

#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(swapchain, acquire, wlr_buffer *, int *age)
#else
    QW_FUNC_MEMBER(swapchain, acquire, wlr_buffer *, void)
#endif

#if WLR_VERSION_MINOR < 19
    QW_FUNC_MEMBER(swapchain, set_buffer_submitted, void, wlr_buffer *buffer)
#endif

private:
    friend class qw_reinterpret_cast;
    QW_FUNC_MEMBER(swapchain, destroy, void)
};

QW_END_NAMESPACE
