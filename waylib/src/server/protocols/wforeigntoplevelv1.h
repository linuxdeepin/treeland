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

class WToplevelSurface;
class WForeignToplevelPrivate;
class WAYLIB_SERVER_EXPORT WForeignToplevel : public QObject, public WServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WForeignToplevel)

public:
    explicit WForeignToplevel(QObject *parent = nullptr);

    void addSurface(WToplevelSurface *surface);
    void removeSurface(WToplevelSurface *surface);

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void requestMaximize(WToplevelSurface *surface, bool isMaximized);
    void requestMinimize(WToplevelSurface *surface, bool isMinimized);
    void requestActivate(WToplevelSurface *surface);
    void requestFullscreen(WToplevelSurface *surface, bool isFullscreen);
    void requestClose(WToplevelSurface *surface);
    void rectangleChanged(WToplevelSurface *surface, const QRect &rect);

private:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
