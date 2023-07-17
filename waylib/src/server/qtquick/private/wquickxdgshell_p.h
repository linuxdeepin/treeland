// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

Q_MOC_INCLUDE(<wseat.h>)

#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WQuickSurface;
class WXdgSurface;
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

    void requestMove(WXdgSurface *surface, WSeat *seat, quint32 serial);
    void requestResize(WXdgSurface *surface, WSeat *seat, Qt::Edges edge, quint32 serial);
    void requestMaximize(WXdgSurface *surface);
    void requestFullscreen(WXdgSurface *surface);
    void requestToNormalState(WXdgSurface *surface);
    void requestShowWindowMenu(WXdgSurface *surface, WSeat *seat, QPoint pos, quint32 serial);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
