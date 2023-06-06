// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxcursormanager.h"

#include <qwxcursormanager.h>

extern "C" {
#include <wlr/types/wlr_xcursor_manager.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WXCursorManagerPrivate : public WObjectPrivate
{
public:
    WXCursorManagerPrivate(WXCursorManager *qq, const char *name, uint32_t size)
        : WObjectPrivate(qq)
        , handle(QWXCursorManager::create(name, size))
    {

    }

    ~WXCursorManagerPrivate() {
        handle->destroy();
    }

    W_DECLARE_PUBLIC(WXCursorManager)

    QWXCursorManager *handle;
};

WXCursorManager::WXCursorManager(uint32_t size, const char *name)
    : WObject(*new WXCursorManagerPrivate(this, name, size))
{

}

WXCursorManagerHandle *WXCursorManager::handle() const
{
    W_DC(WXCursorManager);

    return reinterpret_cast<WXCursorManagerHandle*>(d->handle);
}

bool WXCursorManager::load(float scale)
{
    W_D(WXCursorManager);
    return d->handle->load(scale);
}

WAYLIB_SERVER_END_NAMESPACE
