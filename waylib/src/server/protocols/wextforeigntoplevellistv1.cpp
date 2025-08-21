// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextforeigntoplevellistv1.h"

#include "private/wglobal_p.h"
#include "wglobal.h"
#include "wtoplevelsurface.h"

#include <qwdisplay.h>
#include <qwextforeigntoplevellistv1.h>

#include <map>

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_VERSION 1

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE
Q_LOGGING_CATEGORY(qLcExtForeignToplevel, "waylib.protocols.extforeigntoplevel", QtWarningMsg)

class Q_DECL_HIDDEN WExtForeignToplevelListV1Private : public WObjectPrivate
{
public:
    WExtForeignToplevelListV1Private(WExtForeignToplevelListV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    void add(WToplevelSurface *surface)
    {
        W_Q(WExtForeignToplevelListV1);

        if (surfaces.contains(surface)) {
            qCCritical(qLcExtForeignToplevel)
                << surface << " has been add to ext foreign toplevel list twice";
            return;
        }

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_ext_foreign_toplevel_handle_v1_state state = {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        auto handle = qw_ext_foreign_toplevel_handle_v1::create(
            *q->nativeInterface<qw_ext_foreign_toplevel_list_v1>(), &state);

        surface->safeConnect(&WToplevelSurface::titleChanged, handle, [this, handle, surface] {
            updateState(surface, handle);
        });

        surface->safeConnect(&WToplevelSurface::appIdChanged, handle, [this, handle, surface] {
            updateState(surface, handle);
        });
        
        surfaces.insert({ surface, std::unique_ptr<qw_ext_foreign_toplevel_handle_v1>(handle) });
    }

    void remove(WToplevelSurface *surface)
    {
        surfaces.erase(surface);
    }

    WToplevelSurface *findSurfaceByHandle(qw_ext_foreign_toplevel_handle_v1 *handle) const
    {
        for (const auto &pair : surfaces) {
            if (pair.second.get() == handle) {
                return pair.first;
            }
        }
        return nullptr;
    }

private:
    void updateState(WToplevelSurface *surface, qw_ext_foreign_toplevel_handle_v1 *handle)
    {
        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_ext_foreign_toplevel_handle_v1_state state = {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        handle->update_state(&state);
    }

    W_DECLARE_PUBLIC(WExtForeignToplevelListV1)

    std::map<WToplevelSurface *, std::unique_ptr<qw_ext_foreign_toplevel_handle_v1>> surfaces;
};

WExtForeignToplevelListV1::WExtForeignToplevelListV1([[maybe_unused]] QObject *parent)
    : WObject(*new WExtForeignToplevelListV1Private(this), nullptr)
{
}

void WExtForeignToplevelListV1::addSurface(WToplevelSurface *surface)
{
    W_D(WExtForeignToplevelListV1);

    d->add(surface);
}

void WExtForeignToplevelListV1::removeSurface(WToplevelSurface *surface)
{
    W_D(WExtForeignToplevelListV1);

    d->remove(surface);
}

WToplevelSurface *WExtForeignToplevelListV1::findSurfaceByHandle(qw_ext_foreign_toplevel_handle_v1 *handle) const
{
    W_D(const WExtForeignToplevelListV1);

    return d->findSurfaceByHandle(handle);
}

QByteArrayView WExtForeignToplevelListV1::interfaceName() const
{
    return "ext_foreign_toplevel_list_v1";
}

void WExtForeignToplevelListV1::create(WServer *server)
{
    W_D(WExtForeignToplevelListV1);

    m_handle = qw_ext_foreign_toplevel_list_v1::create(*server->handle(), EXT_FOREIGN_TOPLEVEL_LIST_V1_VERSION);
}

void WExtForeignToplevelListV1::destroy([[maybe_unused]] WServer *server)
{
}

wl_global *WExtForeignToplevelListV1::global() const
{
    return nativeInterface<qw_ext_foreign_toplevel_list_v1>()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
