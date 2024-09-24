// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutputlayout.h>
#include <wsurface.h>

#include <qwglobal.h>
#include <QPointF>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_xcursor_manager;
class qw_input_device;
class qw_cursor;
class qw_output_cursor;
class qw_surface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WInputDevice;
class WXCursorImage;
class WCursorPrivate;
class WAYLIB_SERVER_EXPORT WCursor : public WWrapObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WCursor)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(QCursor cursor READ cursor WRITE setCursor NOTIFY cursorChanged FINAL)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WSurface* requestedDragSurface READ requestedDragSurface NOTIFY requestedDragSurfaceChanged FINAL)
    QML_ANONYMOUS

public:
    typedef WGlobal::CursorShape CursorShape;

    explicit WCursor(QObject *parent = nullptr);

    QW_NAMESPACE::qw_cursor *handle() const;

    static WCursor *fromHandle(const QW_NAMESPACE::qw_cursor *handle);

    static Qt::MouseButton fromNativeButton(uint32_t code);
    static uint32_t toNativeButton(Qt::MouseButton button);
    static QCursor toQCursor(CursorShape shape);

    Qt::MouseButtons state() const;
    Qt::MouseButton button() const;

    WSeat *seat() const;
    QWindow *eventWindow() const;
    void setEventWindow(QWindow *window);

    static Qt::CursorShape defaultCursor();

    QCursor cursor() const;
    void setCursor(const QCursor &cursor);

    // from client
    CursorShape requestedCursorShape() const;
    std::pair<WSurface*, QPoint> requestedCursorSurface() const;
    WSurface* requestedDragSurface() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void setPosition(const QPointF &pos);
    bool setPositionWithChecker(const QPointF &pos);

    bool isVisible() const;
    void setVisible(bool visible);

    QPointF position() const;
    QPointF lastPressedOrTouchDownPosition() const;

Q_SIGNALS:
    void positionChanged();
    void seatChanged();
    void requestedCursorShapeChanged();
    void requestedCursorSurfaceChanged();
    void requestedDragSurfaceChanged();
    void layoutChanged();
    void cursorChanged();
    void visibleChanged();

protected:
    WCursor(WCursorPrivate &dd, QObject *parent = nullptr);
    ~WCursor() override = default;

    virtual void move(QW_NAMESPACE::qw_input_device *device, const QPointF &delta);
    virtual void setPosition(QW_NAMESPACE::qw_input_device *device, const QPointF &pos);
    virtual bool setPositionWithChecker(QW_NAMESPACE::qw_input_device *device, const QPointF &pos);
    virtual void setScalePosition(QW_NAMESPACE::qw_input_device *device, const QPointF &ratio);

private:
    friend class WSeat;
    friend class WSeatPrivate;
    void setSeat(WSeat *seat);
    bool attachInputDevice(WInputDevice *device);
    void detachInputDevice(WInputDevice *device);

    W_PRIVATE_SLOT(void on_swipe_begin(wlr_pointer_swipe_begin_event *event))
    W_PRIVATE_SLOT(void on_swipe_update(wlr_pointer_swipe_update_event *event))
    W_PRIVATE_SLOT(void on_swipe_end(wlr_pointer_swipe_end_event *event))
    W_PRIVATE_SLOT(void on_pinch_begin(wlr_pointer_pinch_begin_event *event))
    W_PRIVATE_SLOT(void on_pinch_update(wlr_pointer_pinch_update_event *event))
    W_PRIVATE_SLOT(void on_pinch_end(wlr_pointer_pinch_end_event *event))
    W_PRIVATE_SLOT(void on_hold_begin(wlr_pointer_hold_begin_event *event))
    W_PRIVATE_SLOT(void on_hold_end(wlr_pointer_hold_end_event *event))
};

WAYLIB_SERVER_END_NAMESPACE
