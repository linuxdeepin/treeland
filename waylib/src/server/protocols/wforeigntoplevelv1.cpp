// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "woutput.h"
#include "wxdgshell.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"
#include "wforeigntoplevelv1.h"

#include <qwforeigntoplevelhandlev1.h>
#include <qwxdgshell.h>
#include <qwdisplay.h>

#include <map>

QW_USE_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WForeignToplevelPrivate : public WObjectPrivate {
public:
    WForeignToplevelPrivate(WForeignToplevel *qq)
        : WObjectPrivate(qq) {}
    ~WForeignToplevelPrivate() {
        for (const auto &pair : std::as_const(connections)) {
            for (const auto &co : std::as_const(pair.second)) {
                QObject::disconnect(co);
            }
        }

        connections.clear();

        for (const auto &i : std::as_const(surfaces)) {
            if (i.second)
                i.second->deleteLater();
        }

        surfaces.clear();
    }

    void initSurface(WXdgSurface *surface) {
        auto handle = surfaces[surface];
        std::vector<QMetaObject::Connection> connection;

        connection.push_back(surface->safeConnect(&WXdgSurface::titleChanged, surface, [=] {
            handle->set_title(surface->title().toUtf8());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::appIdChanged, surface, [=] {
            handle->set_app_id(surface->appId().toUtf8());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::minimizeChanged, surface, [=] {
            handle->set_minimized(surface->isMinimized());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::maximizeChanged, surface, [=] {
            handle->set_maximized(surface->isMaximized());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::fullscreenChanged, surface, [=] {
            handle->set_fullscreen(surface->isFullScreen());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::activateChanged, surface, [=] {
            handle->set_activated(surface->isActivated());
        }));

        connection.push_back(surface->safeConnect(&WXdgSurface::parentSurfaceChanged, surface, [this, surface, handle] {
            auto find = std::find_if(
                surfaces.begin(), surfaces.end(),
                [surface](auto pair) { return pair.first->surface() == surface->parentSurface(); });

            handle->setParent(find != surfaces.end() ? find->second.get() : nullptr);
        }));

        connection.push_back(surface->surface()->safeConnect(&WSurface::outputEntered, surface, [this, handle](WOutput *output) {
            handle->output_enter(output->nativeHandle());
        }));

        connection.push_back(surface->surface()->safeConnect(&WSurface::outputLeft, surface, [this, handle](WOutput *output) {
            handle->output_leave(output->nativeHandle());
        }));

        connection.push_back(QObject::connect(handle.get(),
                            &qw_foreign_toplevel_handle_v1::notify_request_activate,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_activated_event *event) {
                                Q_EMIT q_func()->requestActivate(surface);
                            }));

        connection.push_back(QObject::connect(handle.get(),
                            &qw_foreign_toplevel_handle_v1::notify_request_maximize,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_maximized_event *event) {
                                Q_EMIT q_func()->requestMaximize(surface, event->maximized);
                            }));

        connection.push_back(QObject::connect(handle.get(),
                            &qw_foreign_toplevel_handle_v1::notify_request_minimize,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_minimized_event *event) {
                                Q_EMIT q_func()->requestMinimize(surface, event->minimized);
                            }));

        connection.push_back(QObject::connect(handle.get(),
                            &qw_foreign_toplevel_handle_v1::notify_request_fullscreen,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
                                Q_EMIT q_func()->requestFullscreen(surface, event->fullscreen);
                            }));

        connection.push_back(QObject::connect(handle.get(),
                            &qw_foreign_toplevel_handle_v1::notify_request_close,
                            surface,
                            [surface, this] {
                                Q_EMIT q_func()->requestClose(surface);
                            }));

        Q_EMIT surface->titleChanged();
        Q_EMIT surface->appIdChanged();
        Q_EMIT surface->minimizeChanged();
        Q_EMIT surface->maximizeChanged();
        Q_EMIT surface->fullscreenChanged();
        Q_EMIT surface->activateChanged();
        Q_EMIT surface->parentSurfaceChanged();

        connections.insert({surface, connection});
    }

    void add(WXdgSurface *surface) {
        W_Q(WForeignToplevel);

        auto handle = qw_foreign_toplevel_handle_v1::create(*q->nativeInterface<qw_foreign_toplevel_manager_v1>());
        surfaces.insert({surface, handle});
        initSurface(surface);
    }

    void remove(WXdgSurface *surface) {
        Q_ASSERT(connections.count(surface));

        for (auto co : std::as_const(connections[surface])) {
            QObject::disconnect(co);
        }

        connections.erase(surface);
        surfaces.erase(surface);
    }

    void surfaceOutputEnter(WXdgSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface];
        handle->output_enter(output->nativeHandle());
    }

    void surfaceOutputLeave(WXdgSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface];
        handle->output_leave(output->nativeHandle());
    }

    W_DECLARE_PUBLIC(WForeignToplevel)

    std::map<WXdgSurface*, QPointer<qw_foreign_toplevel_handle_v1>> surfaces;
    std::map<WXdgSurface*, std::vector<QMetaObject::Connection>> connections;
};

WForeignToplevel::WForeignToplevel(QObject *parent)
    : WObject(*new WForeignToplevelPrivate(this), nullptr)
{
}

void WForeignToplevel::addSurface(WXdgSurface *surface)
{
    W_D(WForeignToplevel);

    d->add(surface);
}

void WForeignToplevel::removeSurface(WXdgSurface *surface)
{
    W_D(WForeignToplevel);

    d->remove(surface);
}

QByteArrayView WForeignToplevel::interfaceName() const
{
    return "zwlr_foreign_toplevel_manager_v1";
}

void WForeignToplevel::create(WServer *server)
{
    W_D(WForeignToplevel);

    m_handle = qw_foreign_toplevel_manager_v1::create(*server->handle());
}

void WForeignToplevel::destroy(WServer *server)
{
}

wl_global *WForeignToplevel::global() const
{
    return nativeInterface<qw_foreign_toplevel_manager_v1>()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
