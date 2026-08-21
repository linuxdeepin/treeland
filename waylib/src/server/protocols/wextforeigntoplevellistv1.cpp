// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextforeigntoplevellistv1.h"

#include "private/wglobal_p.h"
#include "wpointer.h"
#include "wtoplevelsurface.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <map>

WAYLIB_SERVER_BEGIN_NAMESPACE
class Q_DECL_HIDDEN WExtForeignToplevelListV1Private : public WObjectPrivate
{
public:
    WExtForeignToplevelListV1Private(WExtForeignToplevelListV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    ~WExtForeignToplevelListV1Private() {
        // The Qt connections live on the surfaces (receiver) while the
        // lambdas capture this private and the raw handles; the manager may
        // be destroyed before the surfaces, so disconnect them all here
        // (mirrors remove()).
        for (const auto &conns : std::as_const(surfaceConnections)) {
            for (const auto &c : conns)
                QObject::disconnect(c);
        }
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
        wlr_ext_foreign_toplevel_handle_v1_state state = {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        auto handle = wlr_ext_foreign_toplevel_handle_v1_create(
            reinterpret_cast<wlr_ext_foreign_toplevel_list_v1*>(q->m_handle), &state);

        QList<QMetaObject::Connection> conns;
        conns.append(QObject::connect(surface, &WToplevelSurface::titleChanged, surface, [this, handle, surface] {
            updateState(surface, handle);
        }));

        conns.append(QObject::connect(surface, &WToplevelSurface::appIdChanged, surface, [this, handle, surface] {
            updateState(surface, handle);
        }));
        surfaceConnections.insert(surface, conns);
        surfaces.insert({ surface, WUniquePointer<wlr_ext_foreign_toplevel_handle_v1>(handle) });

    }

    void remove(WToplevelSurface *surface)
    {
        // The Qt connections live on the surface (receiver) while the lambdas
        // capture the raw wlr handle; disconnect before the handle is freed
        // (mirrors master where the qw_* handle QObject teardown did it).
        const auto conns = surfaceConnections.take(surface);
        for (const auto &c : conns)
            QObject::disconnect(c);
        surfaces.erase(surface);
    }

    WToplevelSurface *findSurfaceByHandle(wlr_ext_foreign_toplevel_handle_v1 *handle) const
    {
        for (const auto &pair : surfaces) {
            if (pair.second.get() == handle) {
                return pair.first;
            }
        }
        return nullptr;
    }

private:
    void updateState(WToplevelSurface *surface, wlr_ext_foreign_toplevel_handle_v1 *handle)
    {
        const auto title = surface->title().toUtf8();
        const auto appId = surface->appId().toLatin1();
        wlr_ext_foreign_toplevel_handle_v1_state state = {
            .title = title.constData(),
            .app_id = appId.constData(),
        };
        wlr_ext_foreign_toplevel_handle_v1_update_state(handle, &state);
    }

    W_DECLARE_PUBLIC(WExtForeignToplevelListV1)

    std::map<WToplevelSurface *, WUniquePointer<wlr_ext_foreign_toplevel_handle_v1>> surfaces;
    QHash<WToplevelSurface *, QList<QMetaObject::Connection>> surfaceConnections;
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

WToplevelSurface *WExtForeignToplevelListV1::findSurfaceByHandle(wlr_ext_foreign_toplevel_handle_v1 *handle) const
{
    W_D(const WExtForeignToplevelListV1);

    return d->findSurfaceByHandle(handle);
}

QByteArrayView WExtForeignToplevelListV1::interfaceName() const
{
    return "ext_foreign_toplevel_list_v1";
}

wlr_ext_foreign_toplevel_list_v1 *WExtForeignToplevelListV1::handle() const
{
    return reinterpret_cast<wlr_ext_foreign_toplevel_list_v1*>(m_handle);
}

void WExtForeignToplevelListV1::create(WServer *server)
{
    m_handle = wlr_ext_foreign_toplevel_list_v1_create(server->handle(), InterfaceVersion);
}

void WExtForeignToplevelListV1::destroy([[maybe_unused]] WServer *server)
{
    // The wlr_ext_foreign_toplevel_list_v1 is reclaimed by display.reset() in
    // WServer::stop(); null m_handle so handle()/global() return null instead
    // of a dangling pointer (kept as an explicit override rather than the
    // inherited empty base for this hardening).
    m_handle = nullptr;
}

wl_global *WExtForeignToplevelListV1::global() const
{
    if (!m_handle)
        return nullptr;
    return reinterpret_cast<wlr_ext_foreign_toplevel_list_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
