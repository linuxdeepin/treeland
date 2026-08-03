// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_primary_selection.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(primary_selection_source)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(seat, request_set_primary_selection, void, wlr_seat *seat, wlr_seat_client *client, wlr_primary_selection_source *source, uint32_t serial)
    QW_FUNC_STATIC(seat, set_primary_selection, void, wlr_seat *seat, wlr_primary_selection_source *source, uint32_t serial)

    QW_FUNC_MEMBER(primary_selection_source, init, void, const wlr_primary_selection_source_impl *impl)
    QW_FUNC_MEMBER(primary_selection_source, send, void, const char *mime_type, int fd)

protected:
    QW_FUNC_MEMBER(primary_selection_source, destroy, void)
};
QW_END_NAMESPACE
