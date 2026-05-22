// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtopleveltagmanager.h"

#include "private/wglobal_p.h"
#include "wsurface.h"
#include "wxdgtoplevelsurface.h"
#include <wayland-server-core.h>

#include <qwdisplay.h>
#include <qwxdgshell.h>

extern "C" {
#include <assert.h>
#include <stdlib.h>
#include "xdg-toplevel-tag-v1-protocol.h"
#include <wlr/types/wlr_xdg_shell.h>
}

// Copy from wlroots
struct Q_DECL_HIDDEN way_xdg_toplevel_tag_manager_v1
{
    struct wl_global *global;

    struct
    {
        struct wl_signal set_tag; // struct way_xdg_toplevel_tag_manager_v1_set_tag_event
        struct wl_signal
            set_description; // struct way_xdg_toplevel_tag_manager_v1_set_description_event
        struct wl_signal destroy;
    } events;

    struct
    {
        struct wl_listener display_destroy;
    } WLR_PRIVATE;
};

struct Q_DECL_HIDDEN way_xdg_toplevel_tag_manager_v1_set_tag_event
{
    struct wlr_xdg_toplevel *toplevel;
    const char *tag;
};

struct Q_DECL_HIDDEN way_xdg_toplevel_tag_manager_v1_set_description_event
{
    struct wlr_xdg_toplevel *toplevel;
    const char *description;
};

struct way_xdg_toplevel_tag_manager_v1 *
way_xdg_toplevel_tag_manager_v1_create(struct wl_display *display, uint32_t version);

#define MANAGER_VERSION 1

extern const struct xdg_toplevel_tag_manager_v1_interface manager_impl;

static struct way_xdg_toplevel_tag_manager_v1 *manager_from_resource(struct wl_resource *resource)
{
    assert(
        wl_resource_instance_of(resource, &xdg_toplevel_tag_manager_v1_interface, &manager_impl));
    return static_cast<way_xdg_toplevel_tag_manager_v1 *>(wl_resource_get_user_data(resource));
}

static void manager_handle_set_tag([[maybe_unused]] struct wl_client *client,
                                   struct wl_resource *manager_resource,
                                   struct wl_resource *toplevel_resource,
                                   const char *tag)
{
    struct way_xdg_toplevel_tag_manager_v1 *manager = manager_from_resource(manager_resource);
    auto *qt_toplevel = QW_NAMESPACE::qw_xdg_toplevel::from_resource(toplevel_resource);
    if (!qt_toplevel) {
        return;
    }
    struct wlr_xdg_toplevel *toplevel = qt_toplevel->handle();

    struct way_xdg_toplevel_tag_manager_v1_set_tag_event event = {
        .toplevel = toplevel,
        .tag = tag,
    };
    wl_signal_emit_mutable(&manager->events.set_tag, &event);
}

static void manager_handle_set_description([[maybe_unused]] struct wl_client *client,
                                           struct wl_resource *manager_resource,
                                           struct wl_resource *toplevel_resource,
                                           const char *description)
{
    struct way_xdg_toplevel_tag_manager_v1 *manager = manager_from_resource(manager_resource);
    auto *qt_toplevel = QW_NAMESPACE::qw_xdg_toplevel::from_resource(toplevel_resource);
    if (!qt_toplevel) {
        return;
    }
    struct wlr_xdg_toplevel *toplevel = qt_toplevel->handle();

    struct way_xdg_toplevel_tag_manager_v1_set_description_event event = {
        .toplevel = toplevel,
        .description = description,
    };
    wl_signal_emit_mutable(&manager->events.set_description, &event);
}

static void manager_handle_destroy([[maybe_unused]] struct wl_client *client, struct wl_resource *manager_resource)
{
    wl_resource_destroy(manager_resource);
}

const struct xdg_toplevel_tag_manager_v1_interface manager_impl = {
    .destroy = manager_handle_destroy,
    .set_toplevel_tag = manager_handle_set_tag,
    .set_toplevel_description = manager_handle_set_description,
};

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
    struct way_xdg_toplevel_tag_manager_v1 *manager =
        static_cast<way_xdg_toplevel_tag_manager_v1 *>(data);

    struct wl_resource *resource =
        wl_resource_create(client, &xdg_toplevel_tag_manager_v1_interface, version, id);
    if (resource == NULL) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void manager_handle_display_destroy(struct wl_listener *listener, [[maybe_unused]] void *data)
{
    struct way_xdg_toplevel_tag_manager_v1 *manager =
        wl_container_of(listener, manager, display_destroy);

    wl_signal_emit_mutable(&manager->events.destroy, NULL);

    assert(wl_list_empty(&manager->events.set_tag.listener_list));
    assert(wl_list_empty(&manager->events.set_description.listener_list));
    assert(wl_list_empty(&manager->events.destroy.listener_list));

    wl_list_remove(&manager->display_destroy.link);
    wl_global_destroy(manager->global);
    free(manager);
}

struct way_xdg_toplevel_tag_manager_v1 *
way_xdg_toplevel_tag_manager_v1_create(struct wl_display *display, uint32_t version)
{
    assert(version <= MANAGER_VERSION);

    struct way_xdg_toplevel_tag_manager_v1 *manager =
        static_cast<way_xdg_toplevel_tag_manager_v1 *>(calloc(1, sizeof(*manager)));
    if (manager == NULL) {
        return NULL;
    }

    manager->global = wl_global_create(display,
                                       &xdg_toplevel_tag_manager_v1_interface,
                                       version,
                                       manager,
                                       manager_bind);
    if (manager->global == NULL) {
        free(manager);
        return NULL;
    }

    wl_signal_init(&manager->events.set_tag);
    wl_signal_init(&manager->events.set_description);
    wl_signal_init(&manager->events.destroy);

    manager->display_destroy.notify = manager_handle_display_destroy;
    wl_display_add_destroy_listener(display, &manager->display_destroy);

    return manager;
}

// Copy end from wlroots

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelTagManagerV1Private : public WObjectPrivate
{
public:
    WXdgToplevelTagManagerV1Private(WXdgToplevelTagManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WXdgToplevelTagManagerV1)

    struct way_xdg_toplevel_tag_manager_v1 *manager{ nullptr };
    struct wl_listener set_tag_listener;
    struct wl_listener set_description_listener;

    static void handle_set_tag([[maybe_unused]]struct wl_listener *listener, void *data)
    {
        auto *event = static_cast<way_xdg_toplevel_tag_manager_v1_set_tag_event *>(data);
        auto *wsurface = WSurface::fromHandle(event->toplevel->base->surface);
        if (!wsurface) {
            return;
        }
        auto *ts = WXdgToplevelSurface::fromSurface(wsurface);
        if (!ts) {
            return;
        }

        ts->setTag(QString::fromUtf8(event->tag));
    }

    static void handle_set_description([[maybe_unused]] struct wl_listener *listener, void *data)
    {
        auto *event = static_cast<way_xdg_toplevel_tag_manager_v1_set_description_event *>(data);
        auto *wsurface = WSurface::fromHandle(event->toplevel->base->surface);
        if (!wsurface) {
            return;
        }
        auto *ts = WXdgToplevelSurface::fromSurface(wsurface);
        if (!ts)
            return;
        ts->setDescription(QString::fromUtf8(event->description));
    }
};

WXdgToplevelTagManagerV1::WXdgToplevelTagManagerV1()
    : WObject(*new WXdgToplevelTagManagerV1Private(this))
{
}

void WXdgToplevelTagManagerV1::create([[maybe_unused]] WServer *wserver)
{
    W_D(WXdgToplevelTagManagerV1);

    d->manager = way_xdg_toplevel_tag_manager_v1_create(*server()->handle(), MANAGER_VERSION);
    m_handle = d->manager;

    if (d->manager) {
        d->set_tag_listener.notify = WXdgToplevelTagManagerV1Private::handle_set_tag;
        wl_signal_add(&d->manager->events.set_tag, &d->set_tag_listener);

        d->set_description_listener.notify =
            WXdgToplevelTagManagerV1Private::handle_set_description;
        wl_signal_add(&d->manager->events.set_description, &d->set_description_listener);
    }
}

void WXdgToplevelTagManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgToplevelTagManagerV1);
    if (d->manager) {
        wl_list_remove(&d->set_tag_listener.link);
        wl_list_remove(&d->set_description_listener.link);
    }
    d->manager = nullptr;
    m_handle = nullptr;
}

wl_global *WXdgToplevelTagManagerV1::global() const
{
    W_DC(WXdgToplevelTagManagerV1);
    if (d->manager)
        return d->manager->global;
    return nullptr;
}

QByteArrayView WXdgToplevelTagManagerV1::interfaceName() const
{
    return "xdg_toplevel_tag_manager_v1";
}

WAYLIB_SERVER_END_NAMESPACE
