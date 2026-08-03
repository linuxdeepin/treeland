// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wcursor.h"
#include "private/wglobal_p.h"

#include <QCursor>
#include <QPointer>

extern "C" {
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_touch.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WCursorPrivate : public WWrapObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq);
    ~WCursorPrivate();

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

    void connectNativeEvents();
    void processCursorMotion(wlr_input_device *device, uint32_t time);

    struct NativeListener {
        using Callback = void (*)(WCursorPrivate *, void *);

        wl_listener listener;
        WCursorPrivate *owner = nullptr;
        Callback callback = nullptr;
    };

    void addListener(NativeListener &listener, wl_signal *signal, NativeListener::Callback callback);
    static void handleNativeEvent(wl_listener *listener, void *data);

    W_DECLARE_PUBLIC(WCursor)

    wlr_cursor *handle = nullptr;
    NativeListener motion;
    NativeListener motionAbsolute;
    NativeListener buttonEvent;
    NativeListener axis;
    NativeListener frame;
    NativeListener swipeBegin;
    NativeListener swipeUpdate;
    NativeListener swipeEnd;
    NativeListener pinchBegin;
    NativeListener pinchUpdate;
    NativeListener pinchEnd;
    NativeListener holdBegin;
    NativeListener holdEnd;
    NativeListener touchDown;
    NativeListener touchMotion;
    NativeListener touchFrame;
    NativeListener touchCancel;
    NativeListener touchUp;
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
};

WAYLIB_SERVER_END_NAMESPACE
