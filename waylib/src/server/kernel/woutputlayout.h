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

#include <QObject>

QT_BEGIN_NAMESPACE
class QPoint;
class QPointF;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
class WOutput;
class WOutputLayoutHandle;
class WOutputLayoutPrivate;
class WOutputLayout : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputLayout)
public:
    explicit WOutputLayout(QObject *parent = nullptr);

    WOutputLayoutHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    virtual void add(WOutput *output);
    virtual void add(WOutput *output, const QPoint &pos);
    virtual void move(WOutput *output, const QPoint &pos);
    virtual void remove(WOutput *output);

    QVector<WOutput*> outputList() const;
    WOutput *at(const QPointF &pos) const;

    void addCursor(WCursor *cursor);
    void removeCursor(WCursor *cursor);
    QVector<WCursor*> cursorList() const;

Q_SIGNALS:
    void outputAdded(WOutput *output);
    void outputRemoved(WOutput *output);
};

WAYLIB_SERVER_END_NAMESPACE
