// Copyright (C) 2023-2026 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdecorationmanager.h"
#include "wsurface.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"

#include <memory>
#include <unordered_map>

extern "C" {
#include <wlr/types/wlr_xdg_decoration_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

static WXdgDecorationManager *XDG_DECORATION_MANAGER = nullptr;

class Q_DECL_HIDDEN WXdgDecorationManagerPrivate : public WObjectPrivate
{
public:
    struct DecorationListeners {
        WNativeListener requestMode;
        WNativeListener destroy;
    };

    WXdgDecorationManagerPrivate(WXdgDecorationManager *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewToplevelDecoration(wlr_xdg_toplevel_decoration_v1 *decoration);
    void updateDecorationMode(wlr_xdg_toplevel_decoration_v1 *decoration);

    WXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const
    {
        return decorations.value(surface, WXdgDecorationManager::Undefined);
    }

    W_DECLARE_PUBLIC(WXdgDecorationManager)

    WNativeListener newDecorationListener;
    std::unordered_map<wlr_xdg_toplevel_decoration_v1 *,
                       std::unique_ptr<DecorationListeners>> decorationListeners;
    WXdgDecorationManager::DecorationMode preferredMode = WXdgDecorationManager::Server;
    QMap<WSurface *, WXdgDecorationManager::DecorationMode> decorations;
};

void WXdgDecorationManagerPrivate::onNewToplevelDecoration(
    wlr_xdg_toplevel_decoration_v1 *decoration)
{
    auto listeners = std::make_unique<DecorationListeners>();
    listeners->requestMode.connect(&decoration->events.request_mode, [this, decoration](void *) {
        updateDecorationMode(decoration);
    });
    listeners->destroy.connect(&decoration->events.destroy, [this, decoration](void *) {
        auto *surface = WSurface::fromHandle(decoration->toplevel->base->surface);
        decorations.remove(surface);
        decorationListeners.erase(decoration);
    });
    decorationListeners.emplace(decoration, std::move(listeners));

    // Some clients don't send request_mode, so initialize from the current request.
    updateDecorationMode(decoration);
}

void WXdgDecorationManagerPrivate::updateDecorationMode(
    wlr_xdg_toplevel_decoration_v1 *decoration)
{
    W_Q(WXdgDecorationManager);

    auto *surface = WSurface::fromHandle(decoration->toplevel->base->surface);
    WXdgDecorationManager::DecorationMode mode = WXdgDecorationManager::Undefined;
    switch (decoration->requested_mode) {
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
            if (decoration->toplevel->base->initialized)
                wlr_xdg_toplevel_decoration_v1_set_mode(
                    decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
            break;
        case WXdgDecorationManager::Server:
            if (decoration->toplevel->base->initialized)
                wlr_xdg_toplevel_decoration_v1_set_mode(
                    decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            break;
        default:
            Q_UNREACHABLE();
        }
    }
    decorations.insert(surface, mode);
    Q_EMIT q->surfaceModeChanged(surface, mode);
}

WXdgDecorationManager::WXdgDecorationManager()
    : WObject(*new WXdgDecorationManagerPrivate(this))
{
    if (XDG_DECORATION_MANAGER)
        qFatal("There are multiple instances of WQuickXdgDecorationManager");
    XDG_DECORATION_MANAGER = this;
}

wlr_xdg_decoration_manager_v1 *WXdgDecorationManager::handle() const
{
    return nativeInterface<wlr_xdg_decoration_manager_v1>();
}

void WXdgDecorationManager::create(WServer *server)
{
    W_D(WXdgDecorationManager);
    auto *manager = wlr_xdg_decoration_manager_v1_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->newDecorationListener.connect(&manager->events.new_toplevel_decoration, [d](void *data) {
        d->onNewToplevelDecoration(static_cast<wlr_xdg_toplevel_decoration_v1 *>(data));
    });
}

void WXdgDecorationManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDecorationManager);
    d->newDecorationListener.disconnect();
    d->decorationListeners.clear();
    d->decorations.clear();
    m_handle = nullptr;
}

wl_global *WXdgDecorationManager::global() const
{
    return handle() ? handle()->global : nullptr;
}

WXdgDecorationManager::DecorationMode WXdgDecorationManager::preferredMode() const
{
    W_DC(WXdgDecorationManager);
    return d->preferredMode;
}

void WXdgDecorationManager::setPreferredMode(DecorationMode mode)
{
    W_D(WXdgDecorationManager);
    if (d->preferredMode == mode)
        return;
    if (d->preferredMode == DecorationMode::Undefined || d->preferredMode == DecorationMode::None) {
        qCWarning(lcWlXdgDecoration, "Prefer mode must be 'Client' or 'Server'");
        return;
    }
    const auto keys = d->decorations.keys();
    for (auto *surface : keys)
        setModeBySurface(surface, mode);
    d->preferredMode = mode;
    Q_EMIT preferredModeChanged(mode);
}

void WXdgDecorationManager::setModeBySurface(WSurface *surface, DecorationMode mode)
{
    W_D(WXdgDecorationManager);
    if (d->modeBySurface(surface) == mode || !handle())
        return;

    wlr_xdg_toplevel_decoration_v1 *decoration;
    wl_list_for_each(decoration, &handle()->decorations, link) {
        if (WSurface::fromHandle(decoration->toplevel->base->surface) != surface)
            continue;
        switch (mode) {
        case WXdgDecorationManager::Client:
            if (decoration->toplevel->base->initialized)
                wlr_xdg_toplevel_decoration_v1_set_mode(
                    decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
            break;
        case WXdgDecorationManager::Server:
            if (decoration->toplevel->base->initialized)
                wlr_xdg_toplevel_decoration_v1_set_mode(
                    decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            break;
        default:
            Q_UNREACHABLE();
        }
        break;
    }
}

WXdgDecorationManager::DecorationMode
WXdgDecorationManager::modeBySurface(WSurface *surface) const
{
    W_DC(WXdgDecorationManager);
    return d->modeBySurface(surface);
}

QByteArrayView WXdgDecorationManager::interfaceName() const
{
    return "zxdg_decoration_manager_v1";
}

WAYLIB_SERVER_END_NAMESPACE
