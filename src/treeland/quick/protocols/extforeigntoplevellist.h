// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "ext-foreign-toplevel-list-server-protocol.h"
#include "ext_foreign_toplevel_list_impl.h"
#include "helper.h"

#include <wxdgsurface.h>
#include <wquickwaylandserver.h>

#include <QObject>
#include <QQmlEngine>

class QWExtForeignToplevelListV1Private;

class QW_EXPORT QWExtForeignToplevelListV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QWExtForeignToplevelListV1)
public:
    inline ext_foreign_toplevel_list_v1 *handle() const
    {
        return QWObject::handle<ext_foreign_toplevel_list_v1>();
    }

    static QWExtForeignToplevelListV1 *get(ext_foreign_toplevel_list_v1 *handle);
    static QWExtForeignToplevelListV1 *from(ext_foreign_toplevel_list_v1 *handle);
    static QWExtForeignToplevelListV1 *create(QWDisplay *display);

    void topLevel(Waylib::Server::WXdgSurface *surface);
    void close(Waylib::Server::WXdgSurface *surface);
    void done(Waylib::Server::WXdgSurface *surface);
    void setTitle(Waylib::Server::WXdgSurface *surface, const QString &title);
    void setAppId(Waylib::Server::WXdgSurface *surface, const QString &appId);
    void setIdentifier(Waylib::Server::WXdgSurface *surface, const QString &identifier);
    void updateSurfaceInfo(Waylib::Server::WXdgSurface *surface);

Q_SIGNALS:
    void beforeDestroy(QWExtForeignToplevelListV1 *self);

private:
    QWExtForeignToplevelListV1(ext_foreign_toplevel_list_v1 *handle, bool isOwner);
    ~QWExtForeignToplevelListV1() = default;
};

class ExtForeignToplevelListPrivate;

class ExtForeignToplevelList : public Waylib::Server::WQuickWaylandServerInterface,
                               public Waylib::Server::WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(ExtForeignToplevelList)

    QML_ELEMENT

public:
    explicit ExtForeignToplevelList(QObject *parent = nullptr);

    Q_INVOKABLE void add(Waylib::Server::WXdgSurface *surface);
    Q_INVOKABLE void remove(Waylib::Server::WXdgSurface *surface);

protected:
    void create() override;
};
