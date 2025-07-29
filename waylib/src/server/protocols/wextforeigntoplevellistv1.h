// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")

QW_BEGIN_NAMESPACE
class qw_ext_foreign_toplevel_handle_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WToplevelSurface;
class WExtForeignToplevelListV1Private;
class WAYLIB_SERVER_EXPORT WExtForeignToplevelListV1 : public QObject, public WServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WExtForeignToplevelListV1)

public:
    explicit WExtForeignToplevelListV1(QObject *parent = nullptr);

    void addSurface(WToplevelSurface *surface);
    void removeSurface(WToplevelSurface *surface); // Must `removeSurface` manually before surface destroy

    // Reverse lookup: find WToplevelSurface from protocol handle
    WToplevelSurface *findSurfaceByHandle(qw_ext_foreign_toplevel_handle_v1 *handle) const;

    QByteArrayView interfaceName() const override;

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
