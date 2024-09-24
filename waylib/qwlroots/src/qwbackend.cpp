// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwbackend.h"

QW_BEGIN_NAMESPACE

qw_backend *qw_backend::create(wlr_backend *handle) {
    if (wlr_backend_is_multi(handle))
        return new qw_multi_backend(handle, false);
#ifdef WLR_HAVE_X11_BACKEND
    if (wlr_backend_is_x11(handle))
        return new qw_x11_backend(handle, false);
#endif
    if (wlr_backend_is_drm(handle))
        return new qw_drm_backend(handle, false);
    if (wlr_backend_is_headless(handle))
        return new qw_headless_backend(handle, false);
    if (wlr_backend_is_libinput(handle))
        return new qw_libinput_backend(handle, false);
    if (wlr_backend_is_wl(handle))
        return new qw_wayland_backend(handle, false);

    return new DeriveType(handle, false);
}

#if WLR_VERSION_MINOR > 17
qw_backend *qw_backend::autocreate(wl_event_loop *event, wlr_session **session_ptr) {
    auto handle = wlr_backend_autocreate(event, session_ptr);
#else
qw_backend *qw_backend::autocreate(wl_display *display, wlr_session **session_ptr) {
    auto handle = wlr_backend_autocreate(display, session_ptr);
#endif
    if (!handle)
        return nullptr;
    if (wlr_backend_is_multi(handle))
        return new qw_multi_backend(handle, true);
#ifdef WLR_HAVE_X11_BACKEND
    if (wlr_backend_is_x11(handle))
        return new qw_x11_backend(handle, true);
#endif
    if (wlr_backend_is_drm(handle))
        return new qw_drm_backend(handle, true);
    if (wlr_backend_is_headless(handle))
        return new qw_headless_backend(handle, true);
    if (wlr_backend_is_libinput(handle))
        return new qw_libinput_backend(handle, true);
    if (wlr_backend_is_wl(handle))
        return new qw_wayland_backend(handle, true);

    return new qw_backend(handle, true);
}

QW_END_NAMESPACE
