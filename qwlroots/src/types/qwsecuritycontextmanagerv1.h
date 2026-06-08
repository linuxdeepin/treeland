// Copyright (C) 2024 YaoBing Xiao <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_security_context_v1.h>
}

QW_BEGIN_NAMESPACE
class QW_CLASS_OBJECT(security_context_manager_v1)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(security_context_manager_v1, create, qw_security_context_manager_v1 *, wl_display *display)

    QW_FUNC_MEMBER(security_context_manager_v1, lookup_client, const wlr_security_context_v1_state *, wl_client *client)
};
QW_END_NAMESPACE
