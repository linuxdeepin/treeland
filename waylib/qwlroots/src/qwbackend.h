// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#define static
#include <wlr/backend/drm.h>
#undef static
#include <wlr/backend/wayland.h>
#ifdef WLR_HAVE_X11_BACKEND
#include <wlr/backend/x11.h>
#endif
#include <wlr/backend/libinput.h>
#include <wlr/backend/headless.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(backend)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(new_output, wlr_output*)
    QW_SIGNAL(new_input, wlr_input_device*)

public:
    static DeriveType *create(wlr_backend *handle);
#if WLR_VERSION_MINOR > 17
    static qw_backend *autocreate(wl_event_loop *event, wlr_session **session_ptr);
#else
    static qw_backend *autocreate(wl_display *display, wlr_session **session_ptr);
#endif

    QW_FUNC_MEMBER(backend, is_multi, bool)
    QW_FUNC_MEMBER(backend, is_drm, bool)
    QW_FUNC_MEMBER(backend, is_wl, bool)
#ifdef WLR_HAVE_X11_BACKEND
    QW_FUNC_MEMBER(backend, is_x11, bool)
#endif
    QW_FUNC_MEMBER(backend, is_libinput, bool)
    QW_FUNC_MEMBER(backend, is_headless, bool)

    QW_FUNC_MEMBER(backend, start, bool)
    QW_FUNC_MEMBER(backend, get_drm_fd, int)

protected:
    QW_FUNC_MEMBER(backend, destroy, void)
};

#define QW_BACKEND(name) \
public: \
    QW_ALWAYS_INLINE static qw_##name *from(wlr_backend *handle) { \
        return qobject_cast<qw_##name *>(qw_backend::from(handle)); \
    } \
protected: \
    using qw_backend::qw_backend; \
    friend class qw_backend;

class QW_EXPORT qw_multi_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(multi_backend)

public:
    QW_FUNC_MEMBER(multi_backend, add, bool, wlr_backend *backend)
    QW_FUNC_MEMBER(multi_backend, remove, void, wlr_backend *backend)
    QW_FUNC_MEMBER(multi, is_empty, bool)
    QW_FUNC_MEMBER(multi, for_each_backend, void, void (*callback)(wlr_backend *backend, void *data), void *data)

    QW_FUNC_STATIC(multi_backend, create, qw_multi_backend *, wl_display *display)
};

class QW_EXPORT qw_drm_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(drm_backend)

public:
    QW_FUNC_STATIC(drm_backend, create, qw_drm_backend *, wl_display *display, wlr_session *session, wlr_device *dev, wlr_backend *parent)

    QW_FUNC_MEMBER(drm_backend, get_non_master_fd, int)

    QW_FUNC_STATIC(drm, connector_get_id, uint32_t, wlr_output *output)
    QW_FUNC_STATIC(drm, connector_add_mode, wlr_output_mode *, wlr_output *output, const drmModeModeInfo *mode)
    QW_FUNC_STATIC(drm, connector_get_panel_orientation, enum wl_output_transform, wlr_output *output)
    QW_FUNC_STATIC(drm, mode_get_info, const drmModeModeInfo *, wlr_output_mode *mode)
    QW_FUNC_STATIC(drm, create_lease, wlr_drm_lease *, wlr_output **outputs, size_t n_outputs, int *lease_fd)
    QW_FUNC_STATIC(drm, lease_terminate, void, wlr_drm_lease *lease)
};

class QW_EXPORT qw_wayland_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(wayland_backend)

public:
    QW_FUNC_MEMBER(wl_backend, create, qw_backend *, wl_display *remote_display)
    QW_FUNC_MEMBER(wl_backend, get_remote_display, wl_display *)
    QW_FUNC_MEMBER(wl, output_create, wlr_output *)
    QW_FUNC_MEMBER(wl, output_create_from_surface, wlr_output *, wl_surface *surface)

    QW_FUNC_STATIC(wl, output_set_title, void, wlr_output *output, const char *title)
    QW_FUNC_STATIC(wl, output_get_surface, wl_surface *, wlr_output *output)
};

#ifdef WLR_HAVE_X11_BACKEND
class QW_EXPORT qw_x11_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(x11_backend)

public:
    QW_FUNC_STATIC(x11_backend, create, qw_x11_backend *, wl_display *display, const char *x11_display)

    QW_FUNC_MEMBER(x11, output_create, wlr_output *)
    QW_FUNC_MEMBER(x11, output_set_title, void, const char *title)
};
#endif

class QW_EXPORT qw_libinput_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(libinput_backend)

public:
    QW_FUNC_STATIC(libinput_backend, create, qw_libinput_backend *, wl_display *display, wlr_session *session)
    QW_FUNC_STATIC(libinput, get_device_handle, libinput_device *, wlr_input_device *dev)
};

class QW_EXPORT qw_headless_backend : public qw_backend
{
    Q_OBJECT
    QW_BACKEND(headless_backend)

public:
    QW_FUNC_STATIC(headless_backend, create, qw_headless_backend *, wl_display *display)

    QW_FUNC_MEMBER(headless, add_output, wlr_output *, unsigned int width, unsigned int height)
};

QW_END_NAMESPACE
