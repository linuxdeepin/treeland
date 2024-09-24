// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "woutput.h"
#include "private/wglobal_p.h"
#include "wforeigntoplevelv1.h"
#include "wtoplevelsurface.h"

#include <qwforeigntoplevelhandlev1.h>
#include <qwxdgshell.h>
#include <qwdisplay.h>

#include <map>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcWlrForeignToplevel, "waylib.protocols.foreigntoplevel", QtWarningMsg)

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
        surfaces.clear();
    }

    void initSurface(WToplevelSurface *surface) {
        auto handle = surfaces[surface].get();
        std::vector<QMetaObject::Connection> connection;

        connection.push_back(surface->safeConnect(&WToplevelSurface::titleChanged, surface, [handle, surface] {
            handle->set_title(surface->title().toUtf8());
        }));

        connection.push_back(surface->safeConnect(&WToplevelSurface::appIdChanged, surface, [handle, surface] {
            handle->set_app_id(surface->appId().toLocal8Bit());
        }));

        connection.push_back(surface->safeConnect(&WToplevelSurface::minimizeChanged, surface, [handle, surface] {
            handle->set_minimized(surface->isMinimized());
        }));

        connection.push_back(surface->safeConnect(&WToplevelSurface::maximizeChanged, surface, [handle, surface] {
            handle->set_maximized(surface->isMaximized());
        }));

        connection.push_back(surface->safeConnect(&WToplevelSurface::fullscreenChanged, surface, [handle, surface] {
            handle->set_fullscreen(surface->isFullScreen());
        }));

        connection.push_back(surface->safeConnect(&WToplevelSurface::activateChanged, surface, [handle, surface] {
            handle->set_activated(surface->isActivated());
        }));

        auto updateSurfaceParent = [this, surface, handle] {
            if (surface->parentSurface()) {
                auto find = std::find_if(surfaces.begin(), surfaces.end(), [surface](const auto &pair) {
                    return pair.first->surface() == surface->parentSurface();
                });
                if (find == surfaces.end()) {
                    qCCritical(qLcWlrForeignToplevel) << "Toplevel surface " << surface
                        << "has set parent surface, but foreign_toplevel_handle for parent surface not found!";
                    return;
                }

                handle->set_parent(*find->second.get());
            }
            else
                handle->set_parent(nullptr);
        };
        connection.push_back(surface->safeConnect(&WToplevelSurface::parentSurfaceChanged, surface, updateSurfaceParent));

        connection.push_back(surface->surface()->safeConnect(&WSurface::outputEntered, surface, [this, handle](WOutput *output) {
            handle->output_enter(output->nativeHandle());
        }));

        connection.push_back(surface->surface()->safeConnect(&WSurface::outputLeft, surface, [this, handle](WOutput *output) {
            handle->output_leave(output->nativeHandle());
        }));

        connection.push_back(QObject::connect(handle,
                            &qw_foreign_toplevel_handle_v1::notify_request_activate,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_activated_event *event) {
                                Q_EMIT q_func()->requestActivate(surface);
                            }));

        connection.push_back(QObject::connect(handle,
                            &qw_foreign_toplevel_handle_v1::notify_request_maximize,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_maximized_event *event) {
                                Q_EMIT q_func()->requestMaximize(surface, event->maximized);
                            }));

        connection.push_back(QObject::connect(handle,
                            &qw_foreign_toplevel_handle_v1::notify_request_minimize,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_minimized_event *event) {
                                Q_EMIT q_func()->requestMinimize(surface, event->minimized);
                            }));

        connection.push_back(QObject::connect(handle,
                            &qw_foreign_toplevel_handle_v1::notify_request_fullscreen,
                            surface,
                            [surface, this](wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
                                Q_EMIT q_func()->requestFullscreen(surface, event->fullscreen);
                            }));

        connection.push_back(QObject::connect(handle,
                            &qw_foreign_toplevel_handle_v1::notify_request_close,
                            surface,
                            [surface, this] {
                                Q_EMIT q_func()->requestClose(surface);
                            }));


        handle->set_title(surface->title().toUtf8());
        handle->set_app_id(surface->appId().toLocal8Bit());
        handle->set_minimized(surface->isMinimized());
        handle->set_maximized(surface->isMaximized());
        handle->set_fullscreen(surface->isFullScreen());
        handle->set_activated(surface->isActivated());
        updateSurfaceParent();

        connections.insert({surface, connection});
    }

    void add(WToplevelSurface *surface) {
        W_Q(WForeignToplevel);

        auto handle = qw_foreign_toplevel_handle_v1::create(*q->nativeInterface<qw_foreign_toplevel_manager_v1>());
        surfaces.insert({surface, std::unique_ptr<qw_foreign_toplevel_handle_v1>(handle)});
        initSurface(surface);
    }

    void remove(WToplevelSurface *surface) {
        Q_ASSERT(connections.count(surface));

        for (auto co : std::as_const(connections[surface])) {
            QObject::disconnect(co);
        }

        connections.erase(surface);
        surfaces.erase(surface);
    }

    void surfaceOutputEnter(WToplevelSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface].get();
        handle->output_enter(output->nativeHandle());
    }

    void surfaceOutputLeave(WToplevelSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface].get();
        handle->output_leave(output->nativeHandle());
    }

    W_DECLARE_PUBLIC(WForeignToplevel)

    std::map<WToplevelSurface*, std::unique_ptr<qw_foreign_toplevel_handle_v1>> surfaces;
    std::map<WToplevelSurface*, std::vector<QMetaObject::Connection>> connections;
};

WForeignToplevel::WForeignToplevel(QObject *parent)
    : WObject(*new WForeignToplevelPrivate(this), nullptr)
{
}

void WForeignToplevel::addSurface(WToplevelSurface *surface)
{
    W_D(WForeignToplevel);

    d->add(surface);
}

void WForeignToplevel::removeSurface(WToplevelSurface *surface)
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
