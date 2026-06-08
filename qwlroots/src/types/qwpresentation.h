// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_presentation_time.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(presentation)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(presentation, create, qw_presentation *, wl_display *display, wlr_backend *backend)

    QW_FUNC_MEMBER(presentation, surface_sampled, wlr_presentation_feedback *, wlr_surface *surface)
    QW_FUNC_MEMBER(presentation, surface_textured_on_output, void, wlr_surface *surface, wlr_output *output)
    QW_FUNC_MEMBER(presentation, surface_scanned_out_on_output, void, wlr_surface *surface, wlr_output *output)
};

class QW_CLASS_REINTERPRET_CAST(presentation_event)
{
public:
    QW_FUNC_MEMBER(presentation_event, from_output, void, const wlr_output_event_present *output_event)
};

class QW_CLASS_REINTERPRET_CAST(presentation_feedback)
{
public:
    QW_FUNC_MEMBER(presentation_feedback, send_presented, void, const wlr_presentation_event *event)

private:
    friend class qw_reinterpret_cast;
    QW_FUNC_MEMBER(presentation_feedback, destroy, void)
};

QW_END_NAMESPACE
