// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXCursorManagerHandle;
class WXCursorManagerPrivate;
class WXCursorManager : public WObject
{
    W_DECLARE_PRIVATE(WXCursorManager)
public:
    WXCursorManager(uint32_t size = 24, const char *name = nullptr);

    WXCursorManagerHandle *handle() const;
    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    bool load(float scale);
};

WAYLIB_SERVER_END_NAMESPACE
