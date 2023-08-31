// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcursorshape_p.h"
#include "wcursor.h"
#include "wseat.h"
#include <qwcursorshapev1.h>
#include <qwseat.h>

extern "C" {
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_seat.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::QWCursorShapeManagerV1;

class WQuickCursorShapeManagerPrivate : public WObjectPrivate
{
public:
    WQuickCursorShapeManagerPrivate(WQuickCursorShapeManager *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WQuickCursorShapeManager)

    QWCursorShapeManagerV1 *manager = nullptr;
};

static inline auto wpToWCursorShape(wp_cursor_shape_device_v1_shape shape) {
    switch (shape) {
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT:
        return WCursor::Default;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU:
        return WCursor::ContextMenu;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP:
        return WCursor::Help;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER:
        return WCursor::Pointer;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS:
        return WCursor::Progress;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT:
        return WCursor::Wait;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL:
        return WCursor::Cell;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR:
        return WCursor::Crosshair;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT:
        return WCursor::Text;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT:
        return WCursor::VerticalText;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS:
        return WCursor::Alias;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY:
        return WCursor::Copy;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE:
        return WCursor::Move;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP:
        return WCursor::NoDrop;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED:
        return WCursor::NotAllowed;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB:
        return WCursor::Grab;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING:
        return WCursor::Grabbing;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE:
        return WCursor::EResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE:
        return WCursor::NResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE:
        return WCursor::NEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE:
        return WCursor::NWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE:
        return WCursor::SResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE:
        return WCursor::SEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE:
        return WCursor::SWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE:
        return WCursor::WResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE:
        return WCursor::EWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE:
        return WCursor::NSResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE:
        return WCursor::NESWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE:
        return WCursor::NWSEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE:
        return WCursor::ColResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE:
        return WCursor::RowResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL:
        return WCursor::AllScroll;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN:
        return WCursor::ZoomIn;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT:
        return WCursor::ZoomOut;
    }
    return WCursor::Invalid;
}

WQuickCursorShapeManager::WQuickCursorShapeManager(QObject *parent):
    WQuickWaylandServerInterface(parent)
  , WObject(*new WQuickCursorShapeManagerPrivate(this), nullptr)
{

}

void WQuickCursorShapeManager::create()
{
    W_D(WQuickCursorShapeManager);
    WQuickWaylandServerInterface::create();

    d->manager = QWCursorShapeManagerV1::create(server()->handle(), 1u);
    connect(d->manager, &QWCursorShapeManagerV1::requestSetShape, this,
            [d] (wlr_cursor_shape_manager_v1_request_set_shape_event *event) {
        if (auto *seat = WSeat::fromHandle(QW_NAMESPACE::QWSeat::from(event->seat_client->seat))) {
            if (seat->cursor())
                seat->cursor()->setCursorShape(wpToWCursorShape(event->shape));
        }
    });
}

WAYLIB_SERVER_END_NAMESPACE
