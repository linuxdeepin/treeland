// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wcursor.h"
#include "private/wglobal_p.h"

#include <QCursor>
#include <QPointer>

struct wlr_pointer;
struct wlr_touch;
struct wlr_cursor;

struct wlr_pointer_motion_event;
struct wlr_pointer_motion_absolute_event;
struct wlr_pointer_button_event;
struct wlr_pointer_axis_event;
struct wlr_pointer_swipe_begin_event;
struct wlr_pointer_swipe_update_event;
struct wlr_pointer_swipe_end_event;
struct wlr_pointer_pinch_begin_event;
struct wlr_pointer_pinch_update_event;
struct wlr_pointer_pinch_end_event;
struct wlr_pointer_hold_begin_event;
struct wlr_pointer_hold_end_event;
struct wlr_touch_down_event;
struct wlr_touch_up_event;
struct wlr_touch_motion_event;
struct wlr_touch_cancel_event;

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WCursorPrivate : public WWrapObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq);
    ~WCursorPrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_cursor)

    void instantRelease() override;

    void sendEnterEvent(WInputDevice *device);
    void sendLeaveEvent(WInputDevice *device);

    // begin slot function
    void on_motion(wlr_pointer_motion_event *event);
    void on_motion_absolute(wlr_pointer_motion_absolute_event *event);
    void on_button(wlr_pointer_button_event *event);
    void on_axis(wlr_pointer_axis_event *event);
    void on_frame();
    void on_swipe_begin(wlr_pointer_swipe_begin_event *event);
    void on_swipe_update(wlr_pointer_swipe_update_event *event);
    void on_swipe_end(wlr_pointer_swipe_end_event *event);
    void on_pinch_begin(wlr_pointer_pinch_begin_event *event);
    void on_pinch_update(wlr_pointer_pinch_update_event *event);
    void on_pinch_end(wlr_pointer_pinch_end_event *event);
    void on_hold_begin(wlr_pointer_hold_begin_event *event);
    void on_hold_end(wlr_pointer_hold_end_event *event);
    void on_touch_down(wlr_touch_down_event *event);
    void on_touch_motion(wlr_touch_motion_event *event);
    void on_touch_frame();
    void on_touch_cancel(wlr_touch_cancel_event *event);
    void on_touch_up(wlr_touch_up_event *event);
    // end slot function

    void connect();
    void removeAllListeners();
    void processCursorMotion(wlr_pointer *device, uint32_t time);

    W_DECLARE_PUBLIC(WCursor)

    QCursor cursor;
    QCursor overrideCursor;

    WSeat *seat = nullptr;
    QPointer<QWindow> eventWindow;
    QPointer<WOutputLayout> outputLayout;
    QList<WInputDevice*> deviceList;

    // for event data
    Qt::MouseButtons state = Qt::NoButton;
    Qt::MouseButton button = Qt::NoButton;
    QPointF lastPressedOrTouchDownPosition;
    bool visible = true;
    double scrollFactor = 1.0;

    WScopedListener m_motionListener;
    WScopedListener m_motionAbsoluteListener;
    WScopedListener m_buttonListener;
    WScopedListener m_axisListener;
    WScopedListener m_frameListener;
    WScopedListener m_swipeBeginListener;
    WScopedListener m_swipeUpdateListener;
    WScopedListener m_swipeEndListener;
    WScopedListener m_pinchBeginListener;
    WScopedListener m_pinchUpdateListener;
    WScopedListener m_pinchEndListener;
    WScopedListener m_holdBeginListener;
    WScopedListener m_holdEndListener;
    WScopedListener m_touchDownListener;
    WScopedListener m_touchMotionListener;
    WScopedListener m_touchFrameListener;
    WScopedListener m_touchCancelListener;
    WScopedListener m_touchUpListener;
};

WAYLIB_SERVER_END_NAMESPACE
