// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wcursorshapemanagerv1.h"
#include "wcursor.h"
#include "wseat.h"
#include "private/wglobal_p.h"

#include <qwcursorshapev1.h>
#include <qwseat.h>
#include <qwdisplay.h>

#define CURSOR_SHAPE_MANAGER_V1_VERSION 1

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::qw_cursor_shape_manager_v1;

class Q_DECL_HIDDEN WCursorShapeManagerV1Private : public WObjectPrivate
{
public:
    WCursorShapeManagerV1Private(WCursorShapeManagerV1 *qq)
        : WObjectPrivate(qq)
    {

    }

    inline qw_cursor_shape_manager_v1 *handle() const {
        return q_func()->nativeInterface<qw_cursor_shape_manager_v1>();
    }

    inline wlr_cursor_shape_manager_v1 *nativeHandle() const {
        Q_ASSERT(handle());
        return handle()->handle();
    }

    W_DECLARE_PUBLIC(WCursorShapeManagerV1)
};

static inline auto wpToWCursorShape(wp_cursor_shape_device_v1_shape shape) {
    switch (shape) {
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT:
        return WGlobal::CursorShape::Default;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU:
        return WGlobal::CursorShape::ContextMenu;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP:
        return WGlobal::CursorShape::Help;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER:
        return WGlobal::CursorShape::Pointer;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS:
        return WGlobal::CursorShape::Progress;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT:
        return WGlobal::CursorShape::Wait;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL:
        return WGlobal::CursorShape::Cell;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR:
        return WGlobal::CursorShape::Crosshair;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT:
        return WGlobal::CursorShape::Text;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT:
        return WGlobal::CursorShape::VerticalText;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS:
        return WGlobal::CursorShape::Alias;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY:
        return WGlobal::CursorShape::Copy;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE:
        return WGlobal::CursorShape::Move;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP:
        return WGlobal::CursorShape::NoDrop;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED:
        return WGlobal::CursorShape::NotAllowed;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB:
        return WGlobal::CursorShape::Grab;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING:
        return WGlobal::CursorShape::Grabbing;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE:
        return WGlobal::CursorShape::EResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE:
        return WGlobal::CursorShape::NResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE:
        return WGlobal::CursorShape::NEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE:
        return WGlobal::CursorShape::NWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE:
        return WGlobal::CursorShape::SResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE:
        return WGlobal::CursorShape::SEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE:
        return WGlobal::CursorShape::SWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE:
        return WGlobal::CursorShape::WResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE:
        return WGlobal::CursorShape::EWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE:
        return WGlobal::CursorShape::NSResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE:
        return WGlobal::CursorShape::NESWResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE:
        return WGlobal::CursorShape::NWSEResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE:
        return WGlobal::CursorShape::ColResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE:
        return WGlobal::CursorShape::RowResize;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL:
        return WGlobal::CursorShape::AllScroll;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN:
        return WGlobal::CursorShape::ZoomIn;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT:
        return WGlobal::CursorShape::ZoomOut;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK:
        return WGlobal::CursorShape::DndAsk;
    case WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE:
        return WGlobal::CursorShape::AllResize;
    }
    return WGlobal::CursorShape::Invalid;
}

WCursorShapeManagerV1::WCursorShapeManagerV1()
    : WObject(*new WCursorShapeManagerV1Private(this))
{

}

qw_cursor_shape_manager_v1 *WCursorShapeManagerV1::handle() const
{
    return nativeInterface<qw_cursor_shape_manager_v1>();
}

QByteArrayView WCursorShapeManagerV1::interfaceName() const
{
    return "wp_cursor_shape_manager_v1";
}

void WCursorShapeManagerV1::create(WServer *server)
{
    W_D(WCursorShapeManagerV1);

    if (!m_handle) {
        m_handle = qw_cursor_shape_manager_v1::create(*server->handle(), CURSOR_SHAPE_MANAGER_V1_VERSION);
        QObject::connect(handle(), &qw_cursor_shape_manager_v1::notify_request_set_shape, this, [this]
                         (wlr_cursor_shape_manager_v1_request_set_shape_event *event) {
            if (auto *seat = WSeat::fromHandle(QW_NAMESPACE::qw_seat::from(event->seat_client->seat))) {
                seat->setCursorShape(event->seat_client, wpToWCursorShape(event->shape));
            }
        });
    }
}

wl_global *WCursorShapeManagerV1::global() const
{
    W_D(const WCursorShapeManagerV1);
    if (m_handle)
        return d->nativeHandle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
