// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgshell.h"
#include "wxdgtoplevelsurface.h"
#include "wxdgpopupsurface.h"
#include "private/wglobal_p.h"

#include <QPointer>

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgShellPrivate : public WObjectPrivate
{
public:
    WXdgShellPrivate(WXdgShell *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewXdgToplevelSurface(wlr_xdg_toplevel *toplevel);
    void onNewXdgPopupSurface(wlr_xdg_popup *popup);
    void releaseSurfaces();

    template<typename Surface>
    void watchSurface(Surface *surface, wlr_xdg_surface *xdgSurface, QVector<Surface *> &surfaces)
    {
        W_Q(WXdgShell);
        auto *surfaceList = &surfaces;
        auto popupListener = std::make_unique<WNativeListener>();
        popupListener->connect(&xdgSurface->events.new_popup, [this](void *data) {
            onNewXdgPopupSurface(static_cast<wlr_xdg_popup *>(data));
        });
        popupListeners.emplace(surface, std::move(popupListener));

        QObject::connect(surface, &WWrapObject::aboutToBeInvalidated, q,
                         [this, surface, surfaceList] {
            if (!surfaceList->removeOne(surface))
                return;
            popupListeners.erase(surface);
            if constexpr (std::is_same_v<Surface, WXdgToplevelSurface>)
                Q_EMIT q_func()->toplevelSurfaceRemoved(surface);
            else
                Q_EMIT q_func()->popupSurfaceRemoved(surface);
        });
    }

    W_DECLARE_PUBLIC(WXdgShell)

    WNativeListener newToplevelListener;
    std::unordered_map<QObject *, std::unique_ptr<WNativeListener>> popupListeners;
    QVector<WXdgToplevelSurface *> toplevelSurfaceList;
    QVector<WXdgPopupSurface *> popupSurfaceList;
    uint32_t version;
};

void WXdgShellPrivate::onNewXdgToplevelSurface(wlr_xdg_toplevel *toplevel)
{
    W_Q(WXdgShell);
    auto *server = q->server();
    auto *surface = new WXdgToplevelSurface(toplevel, server);
    Q_ASSERT(surface->parent() == server);
    toplevelSurfaceList.append(surface);
    watchSurface(surface, toplevel->base, toplevelSurfaceList);
    Q_EMIT q->toplevelSurfaceAdded(surface);
}

void WXdgShellPrivate::onNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_Q(WXdgShell);
    if (WXdgPopupSurface::fromHandle(popup))
        return;

    auto *server = q->server();
    auto *surface = new WXdgPopupSurface(popup, server);
    Q_ASSERT(surface->parent() == server);
    popupSurfaceList.append(surface);
    watchSurface(surface, popup->base, popupSurfaceList);
    Q_EMIT q->popupSurfaceAdded(surface);
}

void WXdgShellPrivate::releaseSurfaces()
{
    const auto toplevels = std::exchange(toplevelSurfaceList, {});
    const auto popups = std::exchange(popupSurfaceList, {});
    popupListeners.clear();

    W_Q(WXdgShell);
    for (auto *surface : toplevels) {
        Q_EMIT q->toplevelSurfaceRemoved(surface);
        surface->safeDeleteLater();
    }
    for (auto *surface : popups) {
        Q_EMIT q->popupSurfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

WXdgShell::WXdgShell(uint32_t version)
    : WObject(*new WXdgShellPrivate(this))
{
    W_D(WXdgShell);
    d->version = version;
}

wlr_xdg_shell *WXdgShell::handle() const
{
    return nativeInterface<wlr_xdg_shell>();
}

QVector<WXdgToplevelSurface *> WXdgShell::toplevelSurfaceList() const
{
    W_DC(WXdgShell);
    return d->toplevelSurfaceList;
}

QByteArrayView WXdgShell::interfaceName() const
{
    return "xdg_wm_base";
}

void WXdgShell::initializeNewXdgPopupSurface(wlr_xdg_popup *popup)
{
    W_D(WXdgShell);
    d->onNewXdgPopupSurface(popup);
}

void WXdgShell::create(WServer *server)
{
    W_D(WXdgShell);
    auto *xdgShell = wlr_xdg_shell_create(server->handle(), d->version);
    Q_ASSERT(xdgShell);
    m_handle = xdgShell;
    d->newToplevelListener.connect(&xdgShell->events.new_toplevel, [d](void *data) {
        d->onNewXdgToplevelSurface(static_cast<wlr_xdg_toplevel *>(data));
    });

    // Popups are observed from their parent xdg_surface. This avoids exposing
    // a popup before the client or layer-shell protocol has assigned a parent.
}

void WXdgShell::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgShell);
    d->newToplevelListener.disconnect();
    d->releaseSurfaces();
    m_handle = nullptr;
}

wl_global *WXdgShell::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
