// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <QPointer>
#include <qpa/qplatformwindow.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WaylibScreen;
class Q_DECL_HIDDEN WaylibOutputWindow : public QPlatformWindow
{
public:
    WaylibOutputWindow(QWindow *window);
    ~WaylibOutputWindow();

    void initialize() override;

    WaylibScreen *waylibScreen() const;
    QPlatformScreen *screen() const override;
    void setGeometry(const QRect &rect) override;
    QRect geometry() const override;

    WId winId() const override;
    qreal devicePixelRatio() const override;

private:
    QMetaObject::Connection onScreenChangedConnection;
    QMetaObject::Connection onScreenGeometryConnection;
};

class WCursor;
class Q_DECL_HIDDEN WaylibRenderWindow : public QPlatformWindow
{
    friend class WaylibCursor;
public:
    WaylibRenderWindow(QWindow *window);

    void initialize() override;
    void setGeometry(const QRect &rect) override;

    WId winId() const override;
    qreal devicePixelRatio() const override;
    void setDevicePixelRatio(qreal dpr);
    bool beforeDisposeEventFilter(QEvent *event);
    bool afterDisposeEventFilter(QEvent *event);

private:
    qreal dpr = 1.0;
    QPointer<WCursor> lastActiveCursor;
};

WAYLIB_SERVER_END_NAMESPACE
