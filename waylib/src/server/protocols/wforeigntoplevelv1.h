// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wglobal.h>
#include <qwglobal.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("wsurface.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgSurface;
class WForeignToplevelPrivate;
class WForeignToplevel : public QObject, public WServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WForeignToplevel)

public:
    explicit WForeignToplevel(QObject *parent = nullptr);

    void addSurface(WXdgSurface *surface);
    void removeSurface(WXdgSurface *surface);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void requestMaximize(WXdgSurface *surface, bool isMaximized);
    void requestMinimize(WXdgSurface *surface, bool isMinimized);
    void requestActivate(WXdgSurface *surface);
    void requestFullscreen(WXdgSurface *surface, bool isFullscreen);
    void requestClose(WXdgSurface *surface);
    void rectangleChanged(WXdgSurface *surface, const QRect &rect);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
