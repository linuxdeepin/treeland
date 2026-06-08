// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_drm_lease_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_REINTERPRET_CAST(drm_lease_v1)
{
public:
    QW_FUNC_MEMBER(drm_lease_v1, revoke, void);
};

class QW_CLASS_REINTERPRET_CAST(drm_lease_request_v1)
{
public:
    QW_FUNC_MEMBER(drm_lease_request_v1, grant, wlr_drm_lease_v1 *);
    QW_FUNC_MEMBER(drm_lease_request_v1, reject, void);
};

class QW_CLASS_OBJECT(drm_lease_v1_manager)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(request, wlr_drm_lease_request_v1*)

public:
    QW_FUNC_STATIC(drm_lease_v1_manager, create, qw_drm_lease_v1_manager *, wl_display *display, wlr_backend *backend);

    QW_FUNC_MEMBER(drm_lease_v1_manager, offer_output, bool, wlr_output *output);
    QW_FUNC_MEMBER(drm_lease_v1_manager, withdraw_output, void, wlr_output *output);
};

QW_END_NAMESPACE
