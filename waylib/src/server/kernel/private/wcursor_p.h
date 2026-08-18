// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "wcursor.h"
#include "private/wglobal_p.h"
#include "wpointer.h"
#include "wscoplistener.h"

#include <wlr_all.h>

#include <QCursor>
#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WCursorPrivate : public WObjectPrivate
{
public:
    WCursorPrivate(WCursor *qq);
    ~WCursorPrivate();

    inline wlr_cursor *handle() const {
        return m_handle.get();
    }

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
    void processCursorMotion(wlr_input_device *device, uint32_t time);

    // Returns true when processCursorMotion should be skipped:
    //   No constraint/non-pointer: return false (caller proceeds).
    //   LOCKED: warp to lockedWarpTarget, return true.
    //   CONFINED: clamp/block, return true if blocked.
    bool applyPointerConstraint(wlr_input_device *device,
                               const QPointF &oldPos,
                               const QPointF &delta);

    // Re-anchor lockedWarpTarget to the current cursor position.
    // Called once at lock activation from setActivePointerConstraint().
    void updateLockedWarpTarget();

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
    // Active constraint enforced on this cursor; set via
    // WCursor::setActivePointerConstraint(). nullptr when unconstrained.
    wlr_pointer_constraint_v1 *activeConstraint = nullptr;
    // Anchor the cursor is warped back to on every motion while a locked
    // constraint is active, preventing physical movement from accumulating.
    QPointF lockedWarpTarget;
    double scrollFactor = 1.0;

private:
    // Owning handle: the cursor is created here and released via
    // wlr_cursor_destroy() in ~WCursorPrivate; WUniquePointer additionally
    // auto-nulls on native destroy, preventing a double-free.
    WUniquePointer<wlr_cursor> m_handle;
};

WAYLIB_SERVER_END_NAMESPACE
