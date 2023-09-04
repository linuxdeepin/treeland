// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxdgdecorationmanager_p.h"
#include <qwxdgdecorationmanagerv1.h>
extern "C" {
#define static
#include <wlr/types/wlr_xdg_decoration_v1.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE
using QW_NAMESPACE::QWXdgDecorationManagerV1;
using QW_NAMESPACE::QWXdgToplevelDecorationV1;

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

    W_DECLARE_PUBLIC(WQuickXdgDecorationManager)

    QWXdgDecorationManagerV1 *manager = nullptr;
    WQuickXdgDecorationManager::DecorationMode mode = WQuickXdgDecorationManager::PreferClientSide;
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
    if (mode == WQuickXdgDecorationManager::PreferClientSide)
        decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    else if (mode == WQuickXdgDecorationManager::PreferServerSide)
        decorat->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    else {
        decorat->setMode(decorat->handle()->requested_mode);
    }
}

WQuickXdgDecorationManager::WQuickXdgDecorationManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickXdgDecorationManagerPrivate(this), nullptr)
{

}

void WQuickXdgDecorationManager::create()
{
    W_D(WQuickXdgDecorationManager);
    d->manager = QWXdgDecorationManagerV1::create(server()->handle());
    connect(d->manager, &QWXdgDecorationManagerV1::newToplevelDecoration, this, [d](QWXdgToplevelDecorationV1 *decorat) {
        d->onNewToplevelDecoration(decorat);
    });
}

void WQuickXdgDecorationManager::setMode(DecorationMode mode)
{
    W_D(WQuickXdgDecorationManager);

    if (d->mode == mode)
        return;
    d->mode = mode;

    // update all existing decoration that mode changed
    if (d->manager) {
        wlr_xdg_decoration_manager_v1 *wlr_manager = d->manager->handle();
        wlr_xdg_toplevel_decoration_v1 *wlr_decorations;
        wl_list_for_each(wlr_decorations, &wlr_manager->decorations, link) {
            d->updateDecorationMode(QWXdgToplevelDecorationV1::from(wlr_decorations));
        }
    }

    Q_EMIT modeChanged(mode);
}

WQuickXdgDecorationManager::DecorationMode WQuickXdgDecorationManager::mode() const
{
    W_DC(WQuickXdgDecorationManager);
    return d->mode;
}

WAYLIB_SERVER_END_NAMESPACE
