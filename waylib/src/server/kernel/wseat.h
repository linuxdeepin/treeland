// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WEvent>
#include <WServer>
#include <WInputDevice>

#include <QEvent>
#include <QSharedData>

Q_MOC_INCLUDE(<wsurface.h>)

QW_BEGIN_NAMESPACE
class qw_seat;
class qw_surface;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QInputEvent;
class QWindow;
class QPointingDevice;
class QQuickItem;
QT_END_NAMESPACE

typedef uint wl_pointer_axis_source_t;
typedef uint wl_pointer_axis_relative_direction_t;
typedef uint wl_pointer_button_state_t;
struct wlr_seat;
struct wlr_seat_client;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WSeat;
class WAYLIB_SERVER_EXPORT WSeatEventFilter : public QObject
{
    friend class WSeat;
    friend class WSeatPrivate;
    Q_OBJECT
public:
    explicit WSeatEventFilter(QObject *parent = nullptr);

protected:
    virtual bool beforeHandleEvent(WSeat *seat, WSurface *watched, QObject *shellObject,
                                        QObject *eventObject, QInputEvent *event);
    virtual bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *shellObject,
                                       QObject *eventObject, QInputEvent *event);
    virtual bool beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event);
    virtual bool unacceptedEvent(WSeat *seat, QWindow *watched, QInputEvent *event);
};

class WCursor;
class WSeatPrivate;
class WAYLIB_SERVER_EXPORT WSeat : public WWrapObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSeat)
    Q_PROPERTY(WInputDevice* keyboard READ keyboard WRITE setKeyboard NOTIFY keyboardChanged FINAL)
    Q_PROPERTY(WSurface* keyboardFocus READ keyboardFocusSurface WRITE setKeyboardFocusSurface NOTIFY keyboardFocusSurfaceChanged FINAL)
    Q_PROPERTY(bool alwaysUpdateHoverTarget READ alwaysUpdateHoverTarget WRITE setAlwaysUpdateHoverTarget NOTIFY alwaysUpdateHoverTargetChanged FINAL)

public:
    WSeat(const QString &name = QStringLiteral("seat0"));

    static WSeat *fromHandle(const QW_NAMESPACE::qw_seat *handle);
    QW_NAMESPACE::qw_seat *handle() const;
    wlr_seat *nativeHandle() const;

    QString name() const;

    void setCursor(WCursor *cursor);
    WCursor *cursor() const;
    void setCursorPosition(const QPointF &pos);
    bool setCursorPositionWithChecker(const QPointF &pos);
    WGlobal::CursorShape requestedCursorShape() const;
    WSurface *requestedCursorSurface() const;
    QPoint requestedCursorSurfaceHotspot() const;
    WSurface *requestedDragSurface() const;

    void attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    // WSurfaceItem is a kind of shellObject
    static bool sendEvent(WSurface *target, QObject *shellObject, QObject *eventObject, QInputEvent *event);
    static WSeat *get(QInputEvent *event);

    WSeatEventFilter *eventFilter() const;
    void setEventFilter(WSeatEventFilter *filter);

    WSurface *pointerFocusSurface() const;

    void setKeyboardFocusSurface(WSurface *surface);
    WSurface *keyboardFocusSurface() const;
    void clearKeyboardFocusSurface();

    void setKeyboardFocusWindow(QWindow *window);
    QWindow *keyboardFocusWindow() const;
    void clearKeyboardFocusWindow();

    WInputDevice *keyboard() const;
    void setKeyboard(WInputDevice *newKeyboard);

    bool alwaysUpdateHoverTarget() const;
    void setAlwaysUpdateHoverTarget(bool newIgnoreSurfacePointerEventExclusiveGrabber);

Q_SIGNALS:
    void keyboardChanged();
    void keyboardFocusSurfaceChanged();
    void requestCursorShape(WAYLIB_SERVER_NAMESPACE::WGlobal::CursorShape shape);
    void requestCursorSurface(WAYLIB_SERVER_NAMESPACE::WSurface *surface, const QPoint &hotspot);
    void requestDrag(WAYLIB_SERVER_NAMESPACE::WSurface *surface);
    void alwaysUpdateHoverTargetChanged();

protected:
    using QObject::eventFilter;
    friend class WOutputPrivate;
    friend class WCursor;
    friend class WCursorPrivate;
    friend class QWlrootsRenderWindow;
    friend class WEventJunkman;
    friend class WCursorShapeManagerV1;
    friend class WOutputRenderWindow;

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

    // for event filter
    bool filterEventBeforeDisposeStage(QWindow *targetWindow, QInputEvent *event);
    bool filterEventBeforeDisposeStage(QQuickItem *target, QInputEvent *event);
    bool filterEventAfterDisposeStage(QWindow *targetWindow, QInputEvent *event);
    bool filterUnacceptedEvent(QWindow *targetWindow, QInputEvent *event);

    // pointer
    void notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp);
    void notifyButton(WCursor *cursor, WInputDevice *device,
                      Qt::MouseButton button, wl_pointer_button_state_t state,
                      uint32_t timestamp);
    void notifyAxis(WCursor *cursor, WInputDevice *device, wl_pointer_axis_source_t source,
                    Qt::Orientation orientation, wl_pointer_axis_relative_direction_t rd,
                    double delta, int32_t delta_discrete, uint32_t timestamp);
    void notifyFrame(WCursor *cursor);

    // gesture
    void notifyGestureBegin(WCursor *cursor, WInputDevice *device, uint32_t time_msec, uint32_t fingers, WGestureEvent::WLibInputGestureType libInputGestureType);
    void notifyGestureUpdate(WCursor *cursor, WInputDevice *device, uint32_t time_msec, const QPointF &delta, double scale, double rotation, WGestureEvent::WLibInputGestureType libInputGestureType);
    void notifyGestureEnd(WCursor *cursor, WInputDevice *device, uint32_t time_msec, bool cancelled, WGestureEvent::WLibInputGestureType libInputGestureType);
    void notifyHoldBegin(WCursor *cursor, WInputDevice *device, uint32_t time_msec, uint32_t fingers);
    void notifyHoldEnd(WCursor *cursor, WInputDevice *device, uint32_t time_msec, bool cancelled);

    // touch
    void notifyTouchDown(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchMotion(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchCancel(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchUp(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchFrame(WCursor *cursor);

    void setCursorShape(wlr_seat_client *client, WGlobal::CursorShape shape);
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSeat*)
