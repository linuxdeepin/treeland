/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>
#include <WSurface>

#include <QObject>

QT_BEGIN_NAMESPACE
class QPoint;
class QPointF;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputLayout;
class WSeat;
class WSurfaceLayoutPrivate;
class WAYLIB_SERVER_EXPORT WSurfaceLayout : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSurfaceLayout)

public:
    explicit WSurfaceLayout(WOutputLayout *outputLayout, QObject *parent = nullptr);

    WOutputLayout *outputLayout() const;
    QVector<WSurface*> surfaceList() const;

    virtual void add(WSurface *surface);
    virtual void remove(WSurface *surface);

    virtual void map(WSurface *surface) = 0;
    virtual void unmap(WSurface *surface) = 0;
    virtual bool isMapped(WSurface *surface) const = 0;

    virtual void requestMove(WSurface *surface, WSeat *seat, quint32 serial);
    virtual void requestResize(WSurface *surface, WSeat *seat, Qt::Edges edge, quint32 serial);
    virtual void requestMaximize(WSurface *surface);
    virtual void requestUnmaximize(WSurface *surface);
    virtual void requestActivate(WSurface *surface, WSeat *seat);

    virtual QPointF position(const WSurface *surface) const;
    virtual bool setPosition(WSurface *surface, const QPointF &pos);
    virtual WOutput *output(WSurface *surface) const;
    virtual bool setOutput(WSurface *surface, WOutput *output);

Q_SIGNALS:
    void surfaceAdded(WSurface *surface);
    void surfaceRemoved(WSurface *surface);

protected:
    WSurfaceLayout(WSurfaceLayoutPrivate &dd, WOutputLayout *outputLayout, QObject *parent = nullptr);
};

WAYLIB_SERVER_END_NAMESPACE
