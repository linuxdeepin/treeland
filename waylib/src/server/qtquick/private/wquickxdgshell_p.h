// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wsurfaceitem.h>

Q_MOC_INCLUDE(<wseat.h>)
Q_MOC_INCLUDE(<wxdgsurface.h>)

#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WQuickSurface;
class WXdgSurface;
class WToplevelSurface;
class WQuickXdgShellPrivate;
class WAYLIB_SERVER_EXPORT WQuickXdgShell : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickXdgShell)
    QML_NAMED_ELEMENT(XdgShell)

public:
    explicit WQuickXdgShell(QObject *parent = nullptr);

Q_SIGNALS:
    void surfaceAdded(WXdgSurface *surface);
    void surfaceRemoved(WXdgSurface *surface);

private:
    WServerInterface *create() override;
};

WAYLIB_SERVER_END_NAMESPACE
