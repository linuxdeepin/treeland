// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextforeigntoplevellistv1.h"

#include "private/wglobal_p.h"
#include "wtoplevelsurface.h"
#include "wayliblogging.h"

extern "C" {
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
}

#include <map>

#define EXT_FOREIGN_TOPLEVEL_LIST_V1_VERSION 1

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN ExtForeignToplevelEntry
{
    wlr_ext_foreign_toplevel_handle_v1 *handle = nullptr;
    QList<QMetaObject::Connection> surfaceConnections;
};

class Q_DECL_HIDDEN WExtForeignToplevelListV1Private : public WObjectPrivate
{
public:
    explicit WExtForeignToplevelListV1Private(WExtForeignToplevelListV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    void add(WToplevelSurface *surface)
    {
        W_Q(WExtForeignToplevelListV1);
        if (surfaces.contains(surface)) {
            qCCritical(lcWlExtForeignToplevel)
                << surface << " has been add to ext foreign toplevel list twice";
            return;
        }

        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_ext_foreign_toplevel_handle_v1_state state {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        auto *handle = wlr_ext_foreign_toplevel_handle_v1_create(q->handle(), &state);
        Q_ASSERT(handle);

        ExtForeignToplevelEntry entry;
        entry.handle = handle;
        entry.surfaceConnections.append(surface->safeConnect(
            &WToplevelSurface::titleChanged, q, [this, surface, handle] {
                updateState(surface, handle);
            }));
        entry.surfaceConnections.append(surface->safeConnect(
            &WToplevelSurface::appIdChanged, q, [this, surface, handle] {
                updateState(surface, handle);
            }));
        surfaces.emplace(surface, std::move(entry));
    }

    void remove(WToplevelSurface *surface)
    {
        const auto it = surfaces.find(surface);
        if (it == surfaces.end())
            return;
        for (const auto &connection : std::as_const(it->second.surfaceConnections))
            surface->safeDisconnect(connection);
        wlr_ext_foreign_toplevel_handle_v1_destroy(it->second.handle);
        surfaces.erase(it);
    }

    void clear()
    {
        while (!surfaces.empty())
            remove(surfaces.begin()->first);
    }

    WToplevelSurface *findSurfaceByHandle(wlr_ext_foreign_toplevel_handle_v1 *handle) const
    {
        for (const auto &[surface, entry] : surfaces) {
            if (entry.handle == handle)
                return surface;
        }
        return nullptr;
    }

    W_DECLARE_PUBLIC(WExtForeignToplevelListV1)

private:
    void updateState(WToplevelSurface *surface, wlr_ext_foreign_toplevel_handle_v1 *handle)
    {
        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_ext_foreign_toplevel_handle_v1_state state {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        wlr_ext_foreign_toplevel_handle_v1_update_state(handle, &state);
    }

    std::map<WToplevelSurface *, ExtForeignToplevelEntry> surfaces;
};

WExtForeignToplevelListV1::WExtForeignToplevelListV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WExtForeignToplevelListV1Private(this))
{
}

void WExtForeignToplevelListV1::addSurface(WToplevelSurface *surface)
{
    d_func()->add(surface);
}

void WExtForeignToplevelListV1::removeSurface(WToplevelSurface *surface)
{
    d_func()->remove(surface);
}

wlr_ext_foreign_toplevel_list_v1 *WExtForeignToplevelListV1::handle() const
{
    return nativeInterface<wlr_ext_foreign_toplevel_list_v1>();
}

WToplevelSurface *WExtForeignToplevelListV1::findSurfaceByHandle(
    wlr_ext_foreign_toplevel_handle_v1 *handle) const
{
    return d_func()->findSurfaceByHandle(handle);
}

QByteArrayView WExtForeignToplevelListV1::interfaceName() const
{
    return "ext_foreign_toplevel_list_v1";
}

void WExtForeignToplevelListV1::create(WServer *server)
{
    m_handle = wlr_ext_foreign_toplevel_list_v1_create(
        server->handle(), EXT_FOREIGN_TOPLEVEL_LIST_V1_VERSION);
    Q_ASSERT(m_handle);
}

void WExtForeignToplevelListV1::destroy([[maybe_unused]] WServer *server)
{
    d_func()->clear();
    m_handle = nullptr;
}

wl_global *WExtForeignToplevelListV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
