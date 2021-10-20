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

#include <WServer>
#include <WInputDevice>

#include <QEvent>
#include <QSharedData>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
class WInputEvent : public QEvent
{
public:
    enum Type {
        PointerMotion,
        PointerButton,
        PointerAxis,
        KeyboardKey,
        KeyboardModifiers
    };

    struct Data : public QSharedData {
        WSeat *seat = nullptr;
        WInputDevice *device = nullptr;
        void *nativeEvent = nullptr;
        WCursor *cursor = nullptr;
        Type eventType;
    };

    using DataPointer = QExplicitlySharedDataPointer<Data>;
    DataPointer data;

    WInputEvent(DataPointer d)
        : QEvent(m_type)
        , data(d)
    {
    }
    ~WInputEvent() {
    }

    inline static QEvent::Type type() {
        return m_type;
    }

private:
    static QEvent::Type m_type;
};

class WSurface;
class WSurfaceHandle;
class WSeatPrivate;
class WSeat : public WServerInterface, public WObject
{
    W_DECLARE_PRIVATE(WSeat)
public:
    WSeat(const QByteArray &name = QByteArrayLiteral("seat0"));

    static WSeat *fromHandle(const void *handle);
    template<typename DNativeInterface>
    static inline WSeat *fromHandle(const DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<const void*>(handle));
    }

    void attachCursor(WCursor *cursor);
    void detachCursor(WCursor *cursor);
    WCursor *attachedCursor() const;

    void attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    void notifyEnterSurface(WSurface *surface, WInputEvent *event);
    void notifyLeaveSurface(WSurface *surface, WInputEvent *event);
    WSurface *hoverSurface() const;

    void setKeyboardFocusTarget(WSurfaceHandle *nativeSurface);
    void setKeyboardFocusTarget(WSurface *surface);

    // pointer
    void notifyMotion(WCursor *cursor, WInputDevice *device,
                      uint32_t timestamp);
    void notifyButton(WCursor *cursor, WInputDevice *device,
                      uint32_t button, WInputDevice::ButtonState state,
                      uint32_t timestamp);
    void notifyAxis(WCursor *cursor, WInputDevice *device,
                    WInputDevice::AxisSource source, Qt::Orientation orientation,
                    double delta, int32_t delta_discrete, uint32_t timestamp);
    void notifyFrame(WCursor *cursor);

    // keyboard
    void notifyKey(WInputDevice *device, uint32_t keycode,
                   WInputDevice::KeyState state, uint32_t timestamp);
    void notifyModifiers(WInputDevice *device);

    QObject *eventGrabber() const;
    void setEventGrabber(QObject *grabber);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;

    friend class WOutputPrivate;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSeat*)
