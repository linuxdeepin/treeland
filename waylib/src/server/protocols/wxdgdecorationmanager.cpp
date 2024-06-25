// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdecorationmanager.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"

#include <qwxdgdecorationmanagerv1.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_decoration_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE
using QW_NAMESPACE::QWXdgDecorationManagerV1;
using QW_NAMESPACE::QWXdgToplevelDecorationV1;

static WXdgDecorationManager *XDG_DECORATION_MANAGER = nullptr;

class WXdgDecorationManagerPrivate : public WObjectPrivate
{
public:
    WXdgDecorationManagerPrivate(WXdgDecorationManager *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewToplevelDecoration(QWXdgToplevelDecorationV1 *decorat);
    // end slot function
    void updateDecorationMode(QWXdgToplevelDecorationV1 *decorat);

    WXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const {
        return decorations.value(surface, WXdgDecorationManager::Undefined);
    }

    inline QWXdgDecorationManagerV1 *handle() const {
        return q_func()->nativeInterface<QWXdgDecorationManagerV1>();
    }

    inline wlr_xdg_decoration_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    W_DECLARE_PUBLIC(WXdgDecorationManager)

    WXdgDecorationManager::DecorationMode preferredMode = WXdgDecorationManager::Server;
    QMap<WSurface*, WXdgDecorationManager::DecorationMode> decorations;
};

void WXdgDecorationManagerPrivate::onNewToplevelDecoration(QWXdgToplevelDecorationV1 *decorat)
{
    W_Q(WXdgDecorationManager);
    QObject::connect(decorat,
                &QWXdgToplevelDecorationV1::requestMode,
                q,
                [decorat, this]() {
                    this->updateDecorationMode(decorat);
                });
    /* For some reason, a lot of clients don't emit the request_mode signal. */
    updateDecorationMode(decorat);
}

void WXdgDecorationManagerPrivate::updateDecorationMode(QWXdgToplevelDecorationV1 *decorat)
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
        switch (preferredMode) {
            case WXdgDecorationManager::Client:
                decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                break;
            case WXdgDecorationManager::Server:
                decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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

    m_handle = QWXdgDecorationManagerV1::create(server->handle());
    connect(d->handle(), &QWXdgDecorationManagerV1::newToplevelDecoration, this, [d](QWXdgToplevelDecorationV1 *decorat) {
        d->onNewToplevelDecoration(decorat);
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
    for (auto *surface : d->decorations.keys()) {
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
                auto * decorat = QWXdgToplevelDecorationV1::from(wlr_decorations);
                switch (mode) {
                    case WXdgDecorationManager::Client:
                        decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                        break;
                    case WXdgDecorationManager::Server:
                        decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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

WAYLIB_SERVER_END_NAMESPACE
