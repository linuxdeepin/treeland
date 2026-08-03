// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_output_layout.h>
#undef static
#include <wlr/util/box.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(output_layout)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(add, wlr_output_layout_output*)
    QW_SIGNAL(change, wlr_output_layout_output*)

public:
#if WLR_VERSION_MINOR > 17
    qw_output_layout(wl_display *display): qw_object(wlr_output_layout_create(display), true, nullptr) { }
#else
    qw_output_layout(QObject *parent = nullptr): qw_object(wlr_output_layout_create(), true, parent) { }
#endif

    QW_FUNC_MEMBER(output_layout, get, wlr_output_layout_output *, wlr_output *reference)
    QW_FUNC_MEMBER(output_layout, output_at, wlr_output *, double lx, double ly)
    QW_FUNC_MEMBER(output_layout, get_center_output, wlr_output *)
    QW_FUNC_MEMBER(output_layout, adjacent_output, wlr_output *, enum wlr_direction direction, wlr_output *reference, double ref_lx, double ref_ly)
    QW_FUNC_MEMBER(output_layout, farthest_output, wlr_output *, enum wlr_direction direction, wlr_output *reference, double ref_lx, double ref_ly)

    QW_FUNC_MEMBER(output_layout, add, wlr_output_layout_output *, wlr_output *output, int lx, int ly)
    QW_FUNC_MEMBER(output_layout, add_auto, wlr_output_layout_output *, wlr_output *output)
    QW_FUNC_MEMBER(output_layout, remove, void, wlr_output *output)

    QW_FUNC_MEMBER(output_layout, output_coords, void, wlr_output *reference, double *lx, double *ly)
    QW_FUNC_MEMBER(output_layout, contains_point, bool, wlr_output *reference, int lx, int ly)
    QW_FUNC_MEMBER(output_layout, intersects, bool, wlr_output *reference, const wlr_box *target_lbox)
    QW_FUNC_MEMBER(output_layout, closest_point, void, wlr_output *reference, double lx, double ly, double *dest_lx, double *dest_ly)
    QW_FUNC_MEMBER(output_layout, get_box, void, wlr_output *reference, wlr_box *dest_box)

protected:
    QW_FUNC_MEMBER(output_layout, destroy, void)
};

QW_END_NAMESPACE
