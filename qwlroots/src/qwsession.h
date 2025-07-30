// Copyright (C) 2025 April Lu <apr3vau@outlook.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwobject.h>

extern "C" {
#include <wlr/backend/session.h>
}

QW_BEGIN_NAMESPACE
class QW_CLASS_OBJECT(device)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(change)
    QW_SIGNAL(remove)
};

class QW_CLASS_OBJECT(session)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(active)
    QW_SIGNAL(add_drm_card)

public:
    QW_FUNC_STATIC(session, create, qw_session *, struct wl_event_loop *loop)

    QW_FUNC_MEMBER(session, open_file, wlr_device *, const char *path)
    QW_FUNC_MEMBER(session, close_file, void, wlr_device *device)
    QW_FUNC_MEMBER(session, change_vt, bool, unsigned vt);
    QW_FUNC_MEMBER(session, find_gpus, ssize_t, size_t ret_len, wlr_device **ret)

protected:
    QW_FUNC_MEMBER(session, destroy, void)
};
QW_END_NAMESPACE
