// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>
#include <WInputDevice>

#include <QEvent>
#include <QSharedData>

QW_BEGIN_NAMESPACE
class QWSeat;
class QWSurface;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QInputEvent;
class QWindow;
class QPointingDevice;
QT_END_NAMESPACE

typedef uint wlr_axis_source_t;
typedef uint wlr_button_state_t;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WSeat;
class WSeatEventFilter : public QObject
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
class WSeat : public WServerInterface, public WObject
{
    W_DECLARE_PRIVATE(WSeat)
public:
    WSeat(const QString &name = QStringLiteral("seat0"));

    static WSeat *fromHandle(const QW_NAMESPACE::QWSeat *handle);
    QW_NAMESPACE::QWSeat *handle() const;

    QString name() const;

    void setCursor(WCursor *cursor);
    WCursor *cursor() const;

    void attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    // WSurfaceItem is a kind of shellObject
    static bool sendEvent(WSurface *target, QObject *shellObject, QObject *eventObject, QInputEvent *event);
    static WSeat *get(QInputEvent *event);

    WSeatEventFilter *eventFilter() const;
    void setEventFilter(WSeatEventFilter *filter);

    WSurface *pointerFocusSurface() const;

    void setKeyboardFocusTarget(QW_NAMESPACE::QWSurface *nativeSurface);
    void setKeyboardFocusTarget(WSurface *surface);
    WSurface *keyboardFocusSurface() const;
    void clearKeyboardFocusSurface();

    void setKeyboardFocusTarget(QWindow *window);
    QWindow *focusWindow() const;
    void clearkeyboardFocusWindow();

protected:
    friend class WOutputPrivate;
    friend class WCursor;
    friend class WCursorPrivate;
    friend class QWlrootsRenderWindow;
    friend class WEventJunkman;

    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

    // for event filter
    bool filterEventBeforeDisposeStage(QWindow *targetWindow, QInputEvent *event);
    bool filterEventAfterDisposeStage(QWindow *targetWindow, QInputEvent *event);
    bool filterUnacceptedEvent(QWindow *targetWindow, QInputEvent *event);

    // pointer
    void notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp);
    void notifyButton(WCursor *cursor, WInputDevice *device,
                      Qt::MouseButton button, wlr_button_state_t state,
                      uint32_t timestamp);
    void notifyAxis(WCursor *cursor, WInputDevice *device, wlr_axis_source_t source,
                    Qt::Orientation orientation,
                    double delta, int32_t delta_discrete, uint32_t timestamp);
    void notifyFrame(WCursor *cursor);

    // touch
    void notifyTouchDown(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchMotion(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchCancel(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchUp(WCursor *cursor, WInputDevice *device, int32_t touch_id, uint32_t time_msec);
    void notifyTouchFrame(WCursor *cursor);
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSeat*)
