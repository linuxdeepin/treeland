// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgtopleveltagmanager.h"

#include "private/wglobal_p.h"
#include "wsurface.h"
#include "wxdgtoplevelsurface.h"
#include <wayland-server-core.h>

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgToplevelTagManagerV1Private : public WObjectPrivate
{
public:
    WXdgToplevelTagManagerV1Private(WXdgToplevelTagManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    W_DECLARE_PUBLIC(WXdgToplevelTagManagerV1)

    struct wlr_xdg_toplevel_tag_manager_v1 *manager{ nullptr };
    struct wl_listener set_tag_listener;
    struct wl_listener set_description_listener;

    static void handle_set_tag([[maybe_unused]]struct wl_listener *listener, void *data)
    {
        auto *event = static_cast<wlr_xdg_toplevel_tag_manager_v1_set_tag_event *>(data);
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
        auto *event = static_cast<wlr_xdg_toplevel_tag_manager_v1_set_description_event *>(data);
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

    d->manager = wlr_xdg_toplevel_tag_manager_v1_create(server()->handle(), InterfaceVersion);
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
