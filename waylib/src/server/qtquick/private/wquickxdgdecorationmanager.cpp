// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgdecorationmanager_p.h"
#include "wsurface.h"
#include "wxdgsurface.h"

#include <qwxdgdecorationmanagerv1.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_decoration_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE
using QW_NAMESPACE::QWXdgDecorationManagerV1;
using QW_NAMESPACE::QWXdgToplevelDecorationV1;

static WQuickXdgDecorationManager *XDG_DECORATION_MANAGER = nullptr;

class WQuickXdgDecorationManagerPrivate : public WObjectPrivate
{
public:
    WQuickXdgDecorationManagerPrivate(WQuickXdgDecorationManager *qq)
        : WObjectPrivate(qq)
    {

    }

    // begin slot function
    void onNewToplevelDecoration(QWXdgToplevelDecorationV1 *decorat);
    // end slot function
    void updateDecorationMode(QWXdgToplevelDecorationV1 *decorat);

    WQuickXdgDecorationManager::DecorationMode modeBySurface(WSurface *surface) const {
        return decorations.value(surface, WQuickXdgDecorationManager::Undefined);
    }

    W_DECLARE_PUBLIC(WQuickXdgDecorationManager)

    QWXdgDecorationManagerV1 *manager = nullptr;
    WQuickXdgDecorationManager::DecorationMode preferredMode = WQuickXdgDecorationManager::Server;
    QMap<WSurface*, WQuickXdgDecorationManager::DecorationMode> decorations;
};

void WQuickXdgDecorationManagerPrivate::onNewToplevelDecoration(QWXdgToplevelDecorationV1 *decorat)
{
    W_Q(WQuickXdgDecorationManager);
    QObject::connect(decorat,
                &QWXdgToplevelDecorationV1::requestMode,
                q,
                [decorat, this]() {
                    this->updateDecorationMode(decorat);
                }
    );
    /* For some reason, a lot of clients don't emit the request_mode signal. */
    updateDecorationMode(decorat);
}

void WQuickXdgDecorationManagerPrivate::updateDecorationMode(QWXdgToplevelDecorationV1 *decorat)
{
    W_Q(WQuickXdgDecorationManager);

    auto *surface = WSurface::fromHandle(decorat->handle()->toplevel->base->surface);
    WQuickXdgDecorationManager::DecorationMode mode = WQuickXdgDecorationManager::Undefined;
    switch (decorat->handle()->requested_mode) {
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE:
            mode = WQuickXdgDecorationManager::None;
            break;
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
            mode = WQuickXdgDecorationManager::Client;
            break;
        case WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
            mode = WQuickXdgDecorationManager::Server;
            break;
        default:
            Q_UNREACHABLE();
            break;
    }
    if (mode == WQuickXdgDecorationManager::None) {
        switch (preferredMode) {
            case WQuickXdgDecorationManager::Client:
                decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                break;
            case WQuickXdgDecorationManager::Server:
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

WQuickXdgDecorationManager::WQuickXdgDecorationManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickXdgDecorationManagerPrivate(this), nullptr)
{
    if (XDG_DECORATION_MANAGER) {
        qFatal("There are multiple instances of WQuickXdgDecorationManager");
    }

    XDG_DECORATION_MANAGER = this;
}

void WQuickXdgDecorationManager::create()
{
    W_D(WQuickXdgDecorationManager);
    WQuickWaylandServerInterface::create();

    d->manager = QWXdgDecorationManagerV1::create(server()->handle());
    connect(d->manager, &QWXdgDecorationManagerV1::newToplevelDecoration, this, [d](QWXdgToplevelDecorationV1 *decorat) {
        d->onNewToplevelDecoration(decorat);
    });
}

WQuickXdgDecorationManager::DecorationMode WQuickXdgDecorationManager::preferredMode() const
{
    W_D(const WQuickXdgDecorationManager);
    return d->preferredMode;
}

void WQuickXdgDecorationManager::setPreferredMode(DecorationMode mode)
{
    W_D(WQuickXdgDecorationManager);
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

void WQuickXdgDecorationManager::setModeBySurface(WSurface *surface, DecorationMode mode)
{
    W_D(WQuickXdgDecorationManager);

    if (d->modeBySurface(surface) == mode) {
        return;
    }

    if (d->manager) {
        wlr_xdg_decoration_manager_v1 *wlr_manager = d->manager->handle();
        wlr_xdg_toplevel_decoration_v1 *wlr_decorations;
        wl_list_for_each(wlr_decorations, &wlr_manager->decorations, link) {
            if (WSurface::fromHandle(wlr_decorations->toplevel->base->surface) == surface) {
                auto * decorat = QWXdgToplevelDecorationV1::from(wlr_decorations);
                switch (mode) {
                    case WQuickXdgDecorationManager::Client:
                        decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
                        break;
                    case WQuickXdgDecorationManager::Server:
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

WQuickXdgDecorationManager::DecorationMode WQuickXdgDecorationManager::modeBySurface(WSurface *surface) const
{
    W_DC(WQuickXdgDecorationManager);
    return d->modeBySurface(surface);
}

WQuickXdgDecorationManagerAttached::WQuickXdgDecorationManagerAttached(WSurface *target, WQuickXdgDecorationManager *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(m_manager, &WQuickXdgDecorationManager::surfaceModeChanged, this, [this] (WSurface *surface, auto mode) {
        if (m_target == surface) {
            Q_EMIT serverDecorationEnabledChanged();
        }
    });
}

bool WQuickXdgDecorationManagerAttached::serverDecorationEnabled() const {
    return m_manager->modeBySurface(m_target) == WQuickXdgDecorationManager::Server;
}

WQuickXdgDecorationManagerAttached *WQuickXdgDecorationManager::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new WQuickXdgDecorationManagerAttached(surface->surface(), XDG_DECORATION_MANAGER);
    }

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
