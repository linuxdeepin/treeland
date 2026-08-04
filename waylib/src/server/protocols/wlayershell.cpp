// Copyright (C) 2023-2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayershell.h"
#include "wlayersurface.h"
#include "wayliblogging.h"
#include "private/wglobal_p.h"
#include "wxdgshell.h"

#include <memory>
#include <unordered_map>
#include <utility>

extern "C" {
#define namespace scope
#include <wlr/types/wlr_layer_shell_v1.h>
#undef namespace
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WLayerShellPrivate : public WObjectPrivate
{
public:
    WLayerShellPrivate(WLayerShell *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewSurface(wlr_layer_surface_v1 *layerSurface);
    void releaseSurfaces();

    W_DECLARE_PUBLIC(WLayerShell)

    WNativeListener newSurfaceListener;
    std::unordered_map<WLayerSurface *, std::unique_ptr<WNativeListener>> popupListeners;
    QVector<WLayerSurface *> surfaceList;
    QPointer<WXdgShell> xdgShell;
};

void WLayerShellPrivate::onNewSurface(wlr_layer_surface_v1 *layerSurface)
{
    W_Q(WLayerShell);
    auto *surface = new WLayerSurface(layerSurface, q->server());
    Q_ASSERT(surface->parent() == q->server());
    surfaceList.append(surface);

    auto popupListener = std::make_unique<WNativeListener>();
    popupListener->connect(&layerSurface->events.new_popup, [this](void *data) {
        if (xdgShell) {
            xdgShell->initializeNewXdgPopupSurface(static_cast<wlr_xdg_popup *>(data));
        } else {
            qCWarning(lcWlLayerShell)
                << "Ignoring layer-surface popup because no xdg-shell manager is attached";
        }
    });
    popupListeners.emplace(surface, std::move(popupListener));

    QObject::connect(surface, &WWrapObject::aboutToBeInvalidated, q, [this, surface] {
        if (!surfaceList.removeOne(surface))
            return;
        popupListeners.erase(surface);
        Q_EMIT q_func()->surfaceRemoved(surface);
    });

    Q_EMIT q->surfaceAdded(surface);
}

void WLayerShellPrivate::releaseSurfaces()
{
    const auto surfaces = std::exchange(surfaceList, {});
    popupListeners.clear();
    W_Q(WLayerShell);
    for (auto *surface : surfaces) {
        Q_EMIT q->surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

WLayerShell::WLayerShell(WXdgShell *xdgShell, QObject *parent)
    : QObject(parent)
    , WObject(*new WLayerShellPrivate(this))
{
    W_D(WLayerShell);
    d->xdgShell = xdgShell;
}

wlr_layer_shell_v1 *WLayerShell::handle() const
{
    return nativeInterface<wlr_layer_shell_v1>();
}

QVector<WLayerSurface *> WLayerShell::surfaceList() const
{
    W_DC(WLayerShell);
    return d->surfaceList;
}

QByteArrayView WLayerShell::interfaceName() const
{
    return "zwlr_layer_shell_v1";
}

void WLayerShell::create(WServer *server)
{
    W_D(WLayerShell);
    auto *layerShell = wlr_layer_shell_v1_create(server->handle(), 4);
    Q_ASSERT(layerShell);
    m_handle = layerShell;
    d->newSurfaceListener.connect(&layerShell->events.new_surface, [d](void *data) {
        d->onNewSurface(static_cast<wlr_layer_surface_v1 *>(data));
    });
}

void WLayerShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WLayerShell);
    d->newSurfaceListener.disconnect();
    d->releaseSurfaces();
    m_handle = nullptr;
}

wl_global *WLayerShell::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
