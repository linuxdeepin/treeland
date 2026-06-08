// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wcursor.h"
#include "private/wglobal_p.h"

#include <qwcursor.h>

#include <QCursor>
#include <QPointer>

QW_BEGIN_NAMESPACE
class qw_pointer;
class qw_surface;
QW_END_NAMESPACE

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
struct wlr_cursor;
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

    WWRAP_HANDLE_FUNCTIONS(QW_NAMESPACE::qw_cursor, wlr_cursor)

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
    void processCursorMotion(QW_NAMESPACE::qw_pointer *device, uint32_t time);

    W_DECLARE_PUBLIC(WCursor)

    QW_NAMESPACE::qw_xcursor_manager *xcursor_manager = nullptr;
    QCursor cursor;

    WSeat *seat = nullptr;
    QPointer<QWindow> eventWindow;
    QPointer<WOutputLayout> outputLayout;
    QList<WInputDevice*> deviceList;

    // for event data
    Qt::MouseButtons state = Qt::NoButton;
    Qt::MouseButton button = Qt::NoButton;
    QPointF lastPressedOrTouchDownPosition;
    bool visible = true;
};

WAYLIB_SERVER_END_NAMESPACE
