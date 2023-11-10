// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "foreign-toplevel-manager-server-protocol.h"
#include "protocols/foreign_toplevel_manager_impl.h"
#include "treeland.h"
#include "treelandhelper.h"

#include <qtmetamacros.h>
#include <wxdgsurface.h>
#include <wquickwaylandserver.h>

#include <QObject>
#include <QQmlEngine>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class QWForeignToplevelManagerV1Private;

class QW_EXPORT QWForeignToplevelManagerV1 : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QWForeignToplevelManagerV1)
public:
    inline ztreeland_foreign_toplevel_manager_v1 *handle() const
    {
        return QWObject::handle<ztreeland_foreign_toplevel_manager_v1>();
    }

    static QWForeignToplevelManagerV1 *get(ztreeland_foreign_toplevel_manager_v1 *handle);
    static QWForeignToplevelManagerV1 *from(ztreeland_foreign_toplevel_manager_v1 *handle);
    static QWForeignToplevelManagerV1 *create(QWDisplay *display);

    void topLevel(Waylib::Server::WXdgSurface *surface);
    void close(Waylib::Server::WXdgSurface *surface);
    void done(Waylib::Server::WXdgSurface *surface);
    void setPid(Waylib::Server::WXdgSurface *surface, uint32_t pid);
    void setIdentifier(Waylib::Server::WXdgSurface *surface, const QString &identifier);
    void updateSurfaceInfo(Waylib::Server::WXdgSurface *surface);

Q_SIGNALS:
    void beforeDestroy(QWForeignToplevelManagerV1 *self);

private:
    QWForeignToplevelManagerV1(ztreeland_foreign_toplevel_manager_v1 *handle, bool isOwner);
    ~QWForeignToplevelManagerV1() = default;
};

class ForeignToplevelManagerPrivate;

class ForeignToplevelManager : public Waylib::Server::WQuickWaylandServerInterface,
                               public Waylib::Server::WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(ForeignToplevelManager)

    QML_ELEMENT

public:
    explicit ForeignToplevelManager(QObject *parent = nullptr);

    Q_INVOKABLE void add(Waylib::Server::WXdgSurface *surface);
    Q_INVOKABLE void remove(Waylib::Server::WXdgSurface *surface);

protected:
    void create() override;
};
