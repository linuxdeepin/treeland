// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdecorationmanager.h"
#include "wsurface.h"
#include "private/wglobal_p.h"

#include <qwxdgdecorationmanagerv1.h>
#include <qwdisplay.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
using QW_NAMESPACE::qw_xdg_decoration_manager_v1;
using QW_NAMESPACE::qw_xdg_toplevel_decoration_v1;

static WXdgDecorationManager *XDG_DECORATION_MANAGER = nullptr;

class Q_DECL_HIDDEN WXdgDecorationManagerPrivate : public WObjectPrivate
{
public:
    WXdgDecorationManagerPrivate(WXdgDecorationManager *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewToplevelDecoration(qw_xdg_toplevel_decoration_v1 *decorat);
    // end slot function
    void updateDecorationMode(qw_xdg_toplevel_decoration_v1 *decorat);

    WXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const {
        return decorations.value(surface, WXdgDecorationManager::Undefined);
    }

    inline qw_xdg_decoration_manager_v1 *handle() const {
        return q_func()->nativeInterface<qw_xdg_decoration_manager_v1>();
    }

    inline wlr_xdg_decoration_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    W_DECLARE_PUBLIC(WXdgDecorationManager)

    WXdgDecorationManager::DecorationMode preferredMode = WXdgDecorationManager::Server;
    QMap<WSurface*, WXdgDecorationManager::DecorationMode> decorations;
};

void WXdgDecorationManagerPrivate::onNewToplevelDecoration(qw_xdg_toplevel_decoration_v1 *decorat)
{
    W_Q(WXdgDecorationManager);
    QObject::connect(decorat,
                &qw_xdg_toplevel_decoration_v1::notify_request_mode,
                q,
                [decorat, this]() {
                    this->updateDecorationMode(decorat);
                });
    /* For some reason, a lot of clients don't emit the request_mode signal. */
    updateDecorationMode(decorat);
}

void WXdgDecorationManagerPrivate::updateDecorationMode(qw_xdg_toplevel_decoration_v1 *decorat)
{
    W_Q(WXdgDecorationManager);

    auto *surface = WSurface::fromHandle(decorat->handle()->toplevel->base->surface);
    WXdgDecorationManager::DecorationMode mode = WXdgDecorationManager::Undefined;
    switch (decorat->handle()->requested_mode) {
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE:
            mode = WXdgDecorationManager::None;
            break;
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
            mode = WXdgDecorationManager::Client;
            break;
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
            mode = WXdgDecorationManager::Server;
            break;
        default:
            Q_UNREACHABLE();
            break;
    }
    if (mode == WXdgDecorationManager::None) {
        mode = preferredMode;
        switch (preferredMode) {
            case WXdgDecorationManager::Client:
                if (decorat->handle()->toplevel->base->initialized)
                    decorat->set_mode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                break;
            case WXdgDecorationManager::Server:
                if (decorat->handle()->toplevel->base->initialized)
                    decorat->set_mode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
                break;
            default:
                Q_UNREACHABLE();
                break;
        }
        decorations.insert(surface, preferredMode);
    } else {
        decorations.insert(surface, mode);
    }

    Q_EMIT q->surfaceModeChanged(surface, mode);
}

WXdgDecorationManager::WXdgDecorationManager()
    :WObject(*new WXdgDecorationManagerPrivate(this))
{
    if (XDG_DECORATION_MANAGER) {
        qFatal("There are multiple instances of WQuickXdgDecorationManager");
    }

    XDG_DECORATION_MANAGER = this;
}

void WXdgDecorationManager::create(WServer *server)
{
    W_D(WXdgDecorationManager);

    m_handle = qw_xdg_decoration_manager_v1::create(*server->handle());
    connect(d->handle(), &qw_xdg_decoration_manager_v1::notify_new_toplevel_decoration, this, [d](wlr_xdg_toplevel_decoration_v1 *decorat) {
        d->onNewToplevelDecoration(qw_xdg_toplevel_decoration_v1::from(decorat));
    });
}

void WXdgDecorationManager::destroy(WServer *server)
{
    Q_UNUSED(server);
}

wl_global *WXdgDecorationManager::global() const
{
    W_D(const WXdgDecorationManager);

    if (m_handle)
        return d->nativeHandle()->global;

    return nullptr;
}

WXdgDecorationManager::DecorationMode WXdgDecorationManager::preferredMode() const
{
    W_D(const WXdgDecorationManager);
    return d->preferredMode;
}

void WXdgDecorationManager::setPreferredMode(DecorationMode mode)
{
    W_D(WXdgDecorationManager);
    if (d->preferredMode == mode) {
        return;
    }
    if (d->preferredMode == DecorationMode::Undefined || d->preferredMode == DecorationMode::None) {
        qWarning("Prefer mode must be 'Client' or 'Server'");
        return;
    }
    // update all existing decoration that mode changed
    const auto keys = d->decorations.keys();
    for (auto *surface : keys) {
        setModeBySurface(surface, mode);
    }
    d->preferredMode = mode;
    Q_EMIT preferredModeChanged(mode);
}

void WXdgDecorationManager::setModeBySurface(WSurface *surface, DecorationMode mode)
{
    W_D(WXdgDecorationManager);

    if (d->modeBySurface(surface) == mode) {
        return;
    }

    if (d->handle()) {
        wlr_xdg_decoration_manager_v1 *wlr_manager = d->nativeHandle();
        wlr_xdg_toplevel_decoration_v1 *wlr_decorations;
        wl_list_for_each(wlr_decorations, &wlr_manager->decorations, link) {
            if (WSurface::fromHandle(wlr_decorations->toplevel->base->surface) == surface) {
                auto * decorat = qw_xdg_toplevel_decoration_v1::from(wlr_decorations);
                switch (mode) {
                    case WXdgDecorationManager::Client:
                        if (decorat->handle()->toplevel->base->initialized)
                            decorat->set_mode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                        break;
                    case WXdgDecorationManager::Server:
                        if (decorat->handle()->toplevel->base->initialized)
                            decorat->set_mode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
                        break;
                    default:
                        Q_UNREACHABLE();
                        break;
                }
                break;
            }
        }
    }
}

WXdgDecorationManager::DecorationMode WXdgDecorationManager::modeBySurface(WSurface *surface) const
{
    W_DC(WXdgDecorationManager);
    return d->modeBySurface(surface);
}

QByteArrayView WXdgDecorationManager::interfaceName() const
{
    return "zxdg_decoration_manager_v1";
}

WAYLIB_SERVER_END_NAMESPACE
