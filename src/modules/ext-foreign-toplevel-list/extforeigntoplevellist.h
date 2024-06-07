// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "server-protocol.h"
#include "helper.h"

#include <wquickwaylandserver.h>
#include <wxdgsurface.h>

#include <QObject>
#include <QQmlEngine>

struct ext_foreign_toplevel_list_v1 : public QObject
{
    Q_OBJECT
public:
    ~ext_foreign_toplevel_list_v1();
    static ext_foreign_toplevel_list_v1 *create(QWDisplay *display);

    void topLevel(Waylib::Server::WXdgSurface *surface);
    void close(Waylib::Server::WXdgSurface *surface);
    void done(Waylib::Server::WXdgSurface *surface);
    void setTitle(Waylib::Server::WXdgSurface *surface, const QString &title);
    void setAppId(Waylib::Server::WXdgSurface *surface, const QString &appId);
    void setIdentifier(Waylib::Server::WXdgSurface *surface, const QString &identifier);
    void updateSurfaceInfo(Waylib::Server::WXdgSurface *surface);

Q_SIGNALS:
    void beforeDestroy(ext_foreign_toplevel_list_v1 *self);

private:
    ext_foreign_toplevel_list_v1(ext_foreign_toplevel_list_v1 *handle, bool isOwner);
    bool init(QWDisplay *display);
    ext_foreign_toplevel_handle_v1 *createToplevelHandle(
        struct wl_resource *client,
        struct wl_resource *surface);
    void on_handleCreated(void *data);

    struct wl_global *m_global;
    std::list<struct wl_resource *> clients;
    std::vector<struct wl_resource *> surfaces;
    std::list<struct ext_foreign_toplevel_handle_v1 *> handles;
    std::vector<Waylib::Server::WXdgSurface *> xdgSurfaces;

    static void ext_foreign_toplevel_list_v1_destroy(struct wl_resource *resource);
    static void ext_foreign_toplevel_list_bind(struct wl_client *client,
                                    void *data,
                                    uint32_t version,
                                    uint32_t id);
    friend struct ext_foreign_toplevel_handle_v1;
};

struct ext_foreign_toplevel_handle_v1
{
    struct ext_foreign_toplevel_list_v1 *manager;
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_resource *surface;
    inline ~ext_foreign_toplevel_handle_v1() {manager->clients.remove(resource);}
};

class ExtForeignToplevelListPrivate;

class ExtForeignToplevelList : public Waylib::Server::WQuickWaylandServerInterface
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit ExtForeignToplevelList(QObject *parent = nullptr);

    Q_INVOKABLE void add(Waylib::Server::WXdgSurface *surface);
    Q_INVOKABLE void remove(Waylib::Server::WXdgSurface *surface);

protected:
    void create() override;
    QScopedPointer<ext_foreign_toplevel_list_v1> manager;
};
