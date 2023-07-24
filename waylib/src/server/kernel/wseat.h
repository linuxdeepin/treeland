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
    virtual bool eventFilter(WSeat *seat, WSurface *watched, QInputEvent *event);
    virtual bool eventFilter(WSeat *seat, QWindow *watched, QInputEvent *event);
    virtual bool ignoredEventFilter(WSeat *seat, QWindow *watched, QInputEvent *event);
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

    static bool sendEvent(WSurface *target, QInputEvent *event);

    WSeatEventFilter *eventFilter() const;
    void setEventFilter(WSeatEventFilter *filter);

    WSurface *pointerEventGrabber() const;
    void setPointerEventGrabber(WSurface *surface);

    WSurface *pointerFocusSurface() const;

    void setKeyboardFocusTarget(QW_NAMESPACE::QWSurface *nativeSurface);
    void setKeyboardFocusTarget(WSurface *surface);
    WSurface *keyboardFocusSurface() const;
    void setKeyboardFocusTarget(QWindow *window);
    QWindow *focusWindow() const;

protected:
    friend class WOutputPrivate;
    friend class WCursor;
    friend class WCursorPrivate;

    void create(WServer *server) override;
    void destroy(WServer *server) override;

    // pointer
    void notifyMotion(WCursor *cursor, WInputDevice *device, uint32_t timestamp);
    void notifyButton(WCursor *cursor, WInputDevice *device,
                      Qt::MouseButton button, wlr_button_state_t state,
                      uint32_t timestamp);
    void notifyAxis(WCursor *cursor, WInputDevice *device, wlr_axis_source_t source,
                    Qt::Orientation orientation,
                    double delta, int32_t delta_discrete, uint32_t timestamp);
    void notifyFrame(WCursor *cursor);
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WSeat*)
