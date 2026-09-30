// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QList>
#include <QObject>
#include <QPointF>
#include <QRectF>

#include <optional>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

// Refcounted handle to a client-provided window-transition source surface.
class WindowTransitionSource : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual WSurface *surface() const = 0;

    virtual void retain(const QList<WOutput *> &outputs) = 0;
    virtual void release(const QList<WOutput *> &outputs) = 0;
};

class WindowTransitionTarget : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual void setWindowTransitionRect(const QRectF &localRect,
                                         WindowTransitionTarget *origin) = 0;
    virtual void updateWindowTransitionRect(const QRectF &localRect) = 0;

    virtual void setWindowTransitionSource(WindowTransitionSource *source) = 0;
    // Drop the live source only
    virtual void clearWindowTransitionSource() = 0;
    // Drop the whole transition and notify the client that the rect is done.
    virtual void resetWindowTransition() = 0;

    virtual std::optional<QPointF> mapTransitionPointToGlobal(
        const QPointF &surfacePoint) const = 0;

Q_SIGNALS:
    void windowTransitionFinished();
};
