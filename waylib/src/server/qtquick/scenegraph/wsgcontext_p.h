// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGContext
{
public:
    // Must run before any QQuickRenderControl is constructed, including the
    // temporary one in WRenderHelper::getGraphicsApi(). If Qt's default
    // context is QSGDefaultContext (RHI), replace it with ours; otherwise
    // keep the adaptation Qt created (software, etc.).
    static void ensureInstalled();
};

WAYLIB_SERVER_END_NAMESPACE
