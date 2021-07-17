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
#include <QPointF>

QT_BEGIN_NAMESPACE
class QCursor;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevice;
class WOutput;
class WOutputLayout;
class WXCursorManager;
class WCursorHandle;
class WCursorPrivate;
class WCursor : public WObject
{
    W_DECLARE_PRIVATE(WCursor)
public:
    WCursor();

    WCursorHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    static WCursor *fromHandle(const WCursorHandle *handle);
    template<typename DNativeInterface>
    static inline WCursor *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const WCursorHandle*>(handle));
    }

    static Qt::MouseButton fromNativeButton(uint32_t code);
    Qt::MouseButtons state() const;
    Qt::MouseButton button() const;

    void setSeat(WSeat *seat);
    WSeat *seat() const;

    static Qt::CursorShape defaultCursor();

    void setManager(WXCursorManager *manager);
    void setType(const char *name);
    void setCursor(const QCursor &cursor);
    void reset();

    bool attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);
    void setOutputLayout(WOutputLayout *layout);

    void mapToOutput(WOutput *output);
    WOutput *mappedOutput() const;

    void setPosition(const QPointF &pos);
    QPointF position() const;
    QPointF lastPressedPosition() const;
};

WAYLIB_SERVER_END_NAMESPACE
