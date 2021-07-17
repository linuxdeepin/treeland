/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wxcursormanager.h"


extern "C" {
#define static
#include <wlr/types/wlr_xcursor_manager.h>
#undef static
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXCursorManagerPrivate : public WObjectPrivate
{
public:
    WXCursorManagerPrivate(WXCursorManager *qq, const char *name, uint32_t size)
        : WObjectPrivate(qq)
        , handle(wlr_xcursor_manager_create(name, size))
    {

    }

    ~WXCursorManagerPrivate() {
        wlr_xcursor_manager_destroy(handle);
    }

    W_DECLARE_PUBLIC(WXCursorManager)

    wlr_xcursor_manager *handle;
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
    return wlr_xcursor_manager_load(d->handle, scale);
}

WAYLIB_SERVER_END_NAMESPACE
