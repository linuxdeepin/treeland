// Copyright (C) 2023-2026 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdecorationmanager.h"
#include "wsurface.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"

#include <wlr/types/wlr_xdg_decoration_v1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE
static WXdgDecorationManager *XDG_DECORATION_MANAGER = nullptr;

class Q_DECL_HIDDEN WXdgDecorationManagerPrivate : public WObjectPrivate
{
public:
    WXdgDecorationManagerPrivate(WXdgDecorationManager *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewToplevelDecoration(wlr_xdg_toplevel_decoration_v1 *decorat);
    // end slot function
    void updateDecorationMode(wlr_xdg_toplevel_decoration_v1 *decorat);
    void cleanupDecorations();

    WXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const {
        return decorations.value(surface, WXdgDecorationManager::Undefined);
    }

    inline wlr_xdg_decoration_manager_v1 *handle() const {
        return static_cast<wlr_xdg_decoration_manager_v1*>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WXdgDecorationManager)

    WScopedListener m_newToplevelDecorationListener;

    struct DecorationState {
        wlr_xdg_toplevel_decoration_v1 *decoration;
        WScopedListener requestModeListener;
        WScopedListener destroyListener;
    };
    QList<DecorationState*> decorationStates;

    WXdgDecorationManager::DecorationMode preferredMode = WXdgDecorationManager::Server;
    QMap<WSurface*, WXdgDecorationManager::DecorationMode> decorations;
};

void WXdgDecorationManagerPrivate::onNewToplevelDecoration(wlr_xdg_toplevel_decoration_v1 *decorat)
{
    auto *state = new DecorationState{decorat, {}, {}};
    decorationStates.append(state);
    state->requestModeListener.connect(&decorat->events.request_mode, [this, decorat](wl_listener *, void *) {
        updateDecorationMode(decorat);
    });
    state->destroyListener.connect(&decorat->events.destroy, [this, state](wl_listener *, void *) {
        decorationStates.removeOne(state);
        delete state;
    });
    /* For some reason, a lot of clients don't Q_EMIT the request_mode signal. */
    updateDecorationMode(decorat);
}

void WXdgDecorationManagerPrivate::cleanupDecorations()
{
    for (auto *state : std::as_const(decorationStates)) {
        delete state;
    }
    decorationStates.clear();
}

void WXdgDecorationManagerPrivate::updateDecorationMode(wlr_xdg_toplevel_decoration_v1 *decorat)
{
    W_Q(WXdgDecorationManager);

    auto *surface = WSurface::fromHandle(decorat->toplevel->base->surface);
    WXdgDecorationManager::DecorationMode mode = WXdgDecorationManager::Undefined;
    switch (decorat->requested_mode) {
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
    }
    if (mode == WXdgDecorationManager::None) {
        mode = preferredMode;
        switch (preferredMode) {
            case WXdgDecorationManager::Client:
                if (decorat->toplevel->base->initialized)
                    wlr_xdg_toplevel_decoration_v1_set_mode(decorat, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                break;
            case WXdgDecorationManager::Server:
                if (decorat->toplevel->base->initialized)
                    wlr_xdg_toplevel_decoration_v1_set_mode(decorat, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
                break;
            default:
                Q_UNREACHABLE();
        }
    }
    decorations.insert(surface, mode);

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

    m_handle = wlr_xdg_decoration_manager_v1_create(server->handle());
    auto *mgr = static_cast<wlr_xdg_decoration_manager_v1*>(m_handle);
    d->m_newToplevelDecorationListener.connect(&mgr->events.new_toplevel_decoration, [d](wl_listener *, void *data) {
        d->onNewToplevelDecoration(static_cast<wlr_xdg_toplevel_decoration_v1*>(data));
    });
}

void WXdgDecorationManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDecorationManager);
    d->m_newToplevelDecorationListener.remove();
    d->cleanupDecorations();
}

wl_global *WXdgDecorationManager::global() const
{
    W_D(const WXdgDecorationManager);

    if (m_handle)
        return d->handle()->global;

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
        qCWarning(lcWlXdgDecoration, "Prefer mode must be 'Client' or 'Server'");
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
        wlr_xdg_decoration_manager_v1 *wlr_manager = d->handle();
        wlr_xdg_toplevel_decoration_v1 *wlr_decorations;
        wl_list_for_each(wlr_decorations, &wlr_manager->decorations, link) {
            if (WSurface::fromHandle(wlr_decorations->toplevel->base->surface) == surface) {
                switch (mode) {
                    case WXdgDecorationManager::Client:
                        if (wlr_decorations->toplevel->base->initialized)
                            wlr_xdg_toplevel_decoration_v1_set_mode(wlr_decorations, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                        break;
                    case WXdgDecorationManager::Server:
                        if (wlr_decorations->toplevel->base->initialized)
                            wlr_xdg_toplevel_decoration_v1_set_mode(wlr_decorations, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
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
