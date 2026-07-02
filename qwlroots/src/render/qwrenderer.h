// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wayland-server-core.h>
#define static
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/pass.h>
#undef static
#if WLR_HAVE_GLES2_RENDERER
#include <wlr/render/gles2.h>
#endif
#include <wlr/render/pixman.h>
#include <wlr/util/box.h>
#if WLR_HAVE_VULKAN_RENDERER
#include <wlr/render/vulkan.h>
#endif
}

struct wlr_renderer;
struct wlr_box;
struct wlr_fbox;
struct wlr_drm_format_set;
struct wl_display;
struct wlr_render_texture_options;

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(renderer)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(lost)

public:
    QW_FUNC_STATIC(renderer, autocreate, qw_renderer *, wlr_backend *backend)

    QW_FUNC_MEMBER(renderer, init_wl_display, bool, wl_display *wl_display)
    QW_FUNC_MEMBER(renderer, init_wl_shm, bool, wl_display *wl_display)
    QW_FUNC_MEMBER(renderer, get_drm_fd, int)
    QW_FUNC_MEMBER(renderer, get_texture_formats, const wlr_drm_format_set *, uint32_t buffer_caps)
#if WLR_HAVE_GLES2_RENDERER
    QW_FUNC_MEMBER(renderer, is_gles2, bool)
#endif
    QW_FUNC_MEMBER(renderer, is_pixman, bool)
#if WLR_HAVE_VULKAN_RENDERER
    QW_FUNC_MEMBER(renderer, is_vk, bool)
    // Access the wlroots-adopted Vulkan device handles. Used by compositors
    // (e.g. waylib) that adopt the wlroots VkDevice into Qt RHI.
    QW_FUNC_MEMBER(vk_renderer, get_instance, VkInstance)
    QW_FUNC_MEMBER(vk_renderer, get_physical_device, VkPhysicalDevice)
    QW_FUNC_MEMBER(vk_renderer, get_device, VkDevice)
    QW_FUNC_MEMBER(vk_renderer, get_queue_family, uint32_t)
#endif

protected:
    QW_FUNC_MEMBER(renderer, destroy, void)
};

QW_END_NAMESPACE
