// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>
#include <WInputDevice>

#include <QEvent>
#include <QSharedData>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
//class WInputEvent : public QEvent
//{
//public:
//    enum Type {
//        PointerMotion,
//        PointerButton,
//        PointerAxis,
//        KeyboardKey,
//        KeyboardModifiers
//    };

//    struct Data : public QSharedData {
//        WSeat *seat = nullptr;
//        WInputDevice *device = nullptr;
//        void *nativeEvent = nullptr;
//        WCursor *cursor = nullptr;
//        Type eventType;
//    };

//    using DataPointer = QExplicitlySharedDataPointer<Data>;
//    DataPointer data;

//    WInputEvent(DataPointer d)
//        : QEvent(m_type)
//        , data(d)
//    {
//    }
//    ~WInputEvent() {
//    }

//    inline static QEvent::Type type() {
//        return m_type;
//    }

//private:
//    static QEvent::Type m_type;
//};

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

    void setCursor(WCursor *cursor);
    WCursor *cursor() const;

    void attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    void notifyEnterSurface(WSurface *surface);
    void notifyLeaveSurface(WSurface *surface);
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
