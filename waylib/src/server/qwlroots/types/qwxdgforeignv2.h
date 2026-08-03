// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>
#include <qwxdgforeignregistry.h>

extern "C" {
#include <wlr/types/wlr_xdg_foreign_v2.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(xdg_foreign_v2)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(xdg_foreign_v2, create, qw_xdg_foreign_v2 *, wl_display *display, wlr_xdg_foreign_registry *registry)
};

class QW_EXPORT qw_xdg_exported_v2 : public qw_xdg_foreign_exported
{
    Q_OBJECT
    using qw_xdg_foreign_exported::qw_xdg_foreign_exported;
};

QW_END_NAMESPACE
