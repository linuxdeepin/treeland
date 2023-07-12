// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

QW_BEGIN_NAMESPACE
class QWXCursorManager;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXCursorManagerPrivate;
class WXCursorManager : public WObject
{
    W_DECLARE_PRIVATE(WXCursorManager)
public:
    WXCursorManager(uint32_t size = 24, const char *name = nullptr);

    QW_NAMESPACE::QWXCursorManager *handle() const;

    bool load(float scale);
};

WAYLIB_SERVER_END_NAMESPACE
