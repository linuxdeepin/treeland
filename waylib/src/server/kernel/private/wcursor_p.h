// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wcursor.h"

#include <QCursor>
#include <QPointer>

QW_BEGIN_NAMESPACE
class QWCursor;
class QWPointer;
class QWSurface;
QW_END_NAMESPACE

struct wlr_pointer_motion_event;
struct wlr_pointer_motion_absolute_event;
struct wlr_pointer_button_event;
struct wlr_pointer_axis_event;
struct wlr_cursor;
struct wlr_touch_down_event;
struct wlr_touch_up_event;
struct wlr_touch_motion_event;
struct wlr_touch_cancel_event;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursorPrivate : public WObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq);
    ~WCursorPrivate();

    wlr_cursor *nativeHandle() const;

    void setType(const char *name);
    void updateCursorImage();

    // begin slot function
    void on_motion(wlr_pointer_motion_event *event);
    void on_motion_absolute(wlr_pointer_motion_absolute_event *event);
    void on_button(wlr_pointer_button_event *event);
    void on_axis(wlr_pointer_axis_event *event);
    void on_frame();
    void on_touch_down(wlr_touch_down_event *event);
    void on_touch_motion(wlr_touch_motion_event *event);
    void on_touch_frame();
    void on_touch_cancel(wlr_touch_cancel_event *event);
    void on_touch_up(wlr_touch_up_event *event);
    // end slot function

    void connect();
    void processCursorMotion(QW_NAMESPACE::QWPointer *device, uint32_t time);

    W_DECLARE_PUBLIC(WCursor)

    QW_NAMESPACE::QWCursor *handle;
    QW_NAMESPACE::QWXCursorManager *xcursor_manager = nullptr;
    QCursor cursor;

    WSeat *seat = nullptr;
    QPointer<QWindow> eventWindow;
    WOutputLayout *outputLayout = nullptr;
    QList<WInputDevice*> deviceList;

    // for event data
    Qt::MouseButtons state = Qt::NoButton;
    Qt::MouseButton button = Qt::NoButton;
    QPointF lastPressedPosition;
    bool visible = true;
    QPointer<QW_NAMESPACE::QWSurface> surfaceOfCursor;
    QPoint surfaceCursorHotspot;
};

WAYLIB_SERVER_END_NAMESPACE
