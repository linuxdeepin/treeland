// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpointergesturesv1.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

wlr_pointer_gestures_v1 *WPointerGesturesV1::handle() const
{
    return nativeInterface<wlr_pointer_gestures_v1>();
}

QByteArrayView WPointerGesturesV1::interfaceName() const
{
    return "zwp_pointer_gestures_v1";
}

void WPointerGesturesV1::create(WServer *server)
{
    if (!qEnvironmentVariableIsSet("WAYLIB_DISABLE_GESTURE"))
        m_handle = wlr_pointer_gestures_v1_create(server->handle());
}

void WPointerGesturesV1::destroy(WServer *)
{
    m_handle = nullptr;
}

wl_global *WPointerGesturesV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
