// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

#include "qwbuffer.h"

extern "C" {
#include <wlr/render/drm_syncobj.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(linux_drm_syncobj_manager_v1)
{
public:
    QW_FUNC_STATIC(linux_drm_syncobj_manager_v1, create, qw_linux_drm_syncobj_manager_v1 *, wl_display *display,
        uint32_t version, int drm_fd)
};

class QW_CLASS_REINTERPRET_CAST(linux_drm_syncobj_surface_v1_state)
{
public:
#if WLR_VERSION_MINOR >= 19
    QW_ALWAYS_INLINE static qw_linux_drm_syncobj_surface_v1_state *get_surface_state(wlr_surface *surface) {
        auto *state = wlr_linux_drm_syncobj_v1_get_surface_state(surface);
        return state ? reinterpret_cast<qw_linux_drm_syncobj_surface_v1_state *>(state) : nullptr;
    }
    QW_ALWAYS_INLINE bool has_acquire_timeline() const {
        return handle() && handle()->acquire_timeline;
    }
    QW_ALWAYS_INLINE uint64_t acquire_point() const {
        return handle() ? handle()->acquire_point : 0;
    }
    QW_ALWAYS_INLINE int export_acquire_sync_file() const {
        return has_acquire_timeline()
            ? wlr_drm_syncobj_timeline_export_sync_file(handle()->acquire_timeline,
                                                        handle()->acquire_point)
            : -1;
    }
    QW_ALWAYS_INLINE bool has_release_timeline() const {
        return handle() && handle()->release_timeline;
    }
    QW_ALWAYS_INLINE uint64_t release_point() const {
        return handle() ? handle()->release_point : 0;
    }
    QW_ALWAYS_INLINE bool signal_release_with_buffer(qw_buffer *buffer) const {
        return handle() && buffer && buffer->handle()
            ? wlr_linux_drm_syncobj_v1_state_signal_release_with_buffer(handle(), buffer->handle())
            : false;
    }
#endif
};

QW_END_NAMESPACE
