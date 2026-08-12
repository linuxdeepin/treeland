// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdecorationmanager.h"
#include "wsurface.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"

#include <wlr_all.h>

#include <memory>
#include <vector>


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

    WXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const {
        return decorations.value(surface, WXdgDecorationManager::Undefined);
    }

    inline wlr_xdg_decoration_manager_v1 *handle() const {
        return reinterpret_cast<wlr_xdg_decoration_manager_v1*>(q_func()->m_handle);
    }

    W_DECLARE_PUBLIC(WXdgDecorationManager)

    WXdgDecorationManager::DecorationMode preferredMode = WXdgDecorationManager::Server;
    QMap<WSurface*, WXdgDecorationManager::DecorationMode> decorations;

    // Per-decoration WListenerOwner: decoration signals are registered on
    // the owner token (not the surface). Manager owns each group via
    // owner->listeners(q), so items stay isolated and manager teardown
    // clears them through the cross-object listener graph.
    struct DecorationListeners {
        wlr_xdg_toplevel_decoration_v1 *decoration = nullptr;
        std::unique_ptr<WListenerOwner> owner;
    };
    std::vector<DecorationListeners> decorationListeners;
};

void WXdgDecorationManagerPrivate::onNewToplevelDecoration(wlr_xdg_toplevel_decoration_v1 *decorat)
{
    W_Q(WXdgDecorationManager);
    auto *surface = WSurface::fromHandle(decorat->toplevel->base->surface);
    if (!surface)
        return;

    DecorationListeners entry;
    entry.decoration = decorat;
    entry.owner = std::make_unique<WListenerOwner>();
    auto *owner = entry.owner.get();

    // Listen to decoration signals on the owner token; q (the manager) owns
    // the group so teardown()/removeListeners(q) detaches without touching
    // other decorations' owners.
    owner->listeners(q)->add(&decorat->events.request_mode, this,
        [decorat, this] (void *) {
        updateDecorationMode(decorat);
    });
    owner->listeners(q)->add(&decorat->events.destroy, this,
        [this, decorat, surface, owner, q] (void *) {
        // Drop our own entry: safe from inside a callback of this list
        // (the closure outlives the emission); the request_mode listener
        // goes with it. wlr_xdg_toplevel_decoration destroy asserts the
        // listener lists are empty after emitting.
        if (decorations.contains(surface))
            decorations.remove(surface);
        owner->removeListeners(q);
        for (auto it = decorationListeners.begin(); it != decorationListeners.end(); ++it) {
            if (it->decoration == decorat) {
                decorationListeners.erase(it);
                break;
            }
        }
    });
    decorationListeners.push_back(std::move(entry));
    /* For some reason, a lot of clients don't emit the request_mode signal. */
    updateDecorationMode(decorat);
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
    listeners()->add(&d->handle()->events.new_toplevel_decoration, d,
        &WXdgDecorationManagerPrivate::onNewToplevelDecoration);
}

void WXdgDecorationManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDecorationManager);
    // Manager-owned listeners were already dropped by WServer teardown.
    // Clearing decorationListeners destroys per-decoration WListenerOwner
    // tokens (their dtors teardown surface-scoped groups).
    d->decorationListeners.clear();
    d->decorations.clear();
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

wlr_xdg_decoration_manager_v1 *WXdgDecorationManager::handle() const
{
    return reinterpret_cast<wlr_xdg_decoration_manager_v1*>(m_handle);
}

WAYLIB_SERVER_END_NAMESPACE
