// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "xdgdialogmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "qwayland-server-xdg-dialog-v1.h"

#include <wserver.h>
#include <wsurface.h>

#include <qwdisplay.h>
#include <qwxdgshell.h>

#include <QPointer>
#include <QSet>

extern "C" {
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_USE_NAMESPACE

class XdgDialogManagerInterfaceV1Private;

class XdgDialogV1Resource : public QtWaylandServer::xdg_dialog_v1
{
public:
    XdgDialogV1Resource(XdgDialogManagerInterfaceV1Private *manager,
                        WSurface *surface,
                        struct ::wl_resource *toplevelResource,
                        struct ::wl_resource *resource)
        : QtWaylandServer::xdg_dialog_v1(resource)
        , m_manager(manager)
        , m_surface(surface)
        , m_toplevelResource(toplevelResource)
        , m_modal(false)
    {
    }

    WSurface *surface() const { return m_surface; }
    bool modal() const { return m_modal; }
    struct ::wl_resource *toplevelResource() const { return m_toplevelResource; }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override;
    void set_modal(Resource *) override;
    void unset_modal(Resource *) override;

private:
    XdgDialogManagerInterfaceV1Private *m_manager;
    QPointer<WSurface> m_surface;
    struct ::wl_resource *m_toplevelResource;
    uint m_modal : 1;
};

class XdgDialogManagerInterfaceV1Private : public QtWaylandServer::xdg_wm_dialog_v1
{
public:
    explicit XdgDialogManagerInterfaceV1Private(XdgDialogManagerInterfaceV1 *q)
        : QtWaylandServer::xdg_wm_dialog_v1()
        , q(q)
    {
    }

    wl_global *globalHandle() const { return m_global; }

    void emitModalChanged(WSurface *surface, bool modal)
    {
        Q_EMIT q->surfaceModalChanged(surface, modal);
    }

    void registerToplevel(struct ::wl_resource *toplevelResource)
    {
        m_usedToplevels.insert(toplevelResource);
    }

    void unregisterToplevel(struct ::wl_resource *toplevelResource)
    {
        m_usedToplevels.remove(toplevelResource);
    }

protected:
    void destroy_global() override
    {
        qCDebug(lcTlXdgDialog) << "xdg_wm_dialog_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_xdg_dialog(Resource *resource, uint32_t id, struct ::wl_resource *toplevel) override
    {
        auto *wlrToplevel = wlr_xdg_toplevel_from_resource(toplevel);
        if (!wlrToplevel) {
            qCWarning(lcTlXdgDialog) << "get_xdg_dialog: invalid xdg_toplevel resource";
            return;
        }

        if (m_usedToplevels.contains(toplevel)) {
            qCWarning(lcTlXdgDialog) << "get_xdg_dialog: xdg_toplevel already has a xdg_dialog_v1";
            wl_resource_post_error(resource->handle,
                                   error_already_used,
                                   "xdg_toplevel already has a xdg_dialog_v1");
            return;
        }

        auto *wlrXdgSurface = wlrToplevel->base;
        auto *wlrSurface = wlrXdgSurface->surface;
        auto *wsurface = WSurface::fromHandle(wlrSurface);
        if (!wsurface) {
            qCWarning(lcTlXdgDialog) << "get_xdg_dialog: no WSurface for xdg_toplevel";
            return;
        }

        auto *dialogResource = wl_resource_create(resource->client(),
                                                   &xdg_dialog_v1_interface,
                                                   wl_resource_get_version(resource->handle),
                                                   id);
        if (!dialogResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        registerToplevel(toplevel);
        new XdgDialogV1Resource(this, wsurface, toplevel, dialogResource);
    }

private:
    XdgDialogManagerInterfaceV1 *q;
    QSet<struct ::wl_resource *> m_usedToplevels;
};

void XdgDialogV1Resource::destroy_resource(Resource *)
{
    m_manager->unregisterToplevel(m_toplevelResource);
    delete this;
}

void XdgDialogV1Resource::set_modal(Resource *)
{
    if (m_modal)
        return;
    m_modal = true;
    if (m_surface)
        m_manager->emitModalChanged(m_surface, true);
}

void XdgDialogV1Resource::unset_modal(Resource *)
{
    if (!m_modal)
        return;
    m_modal = false;
    if (m_surface)
        m_manager->emitModalChanged(m_surface, false);
}

XdgDialogManagerInterfaceV1::XdgDialogManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new XdgDialogManagerInterfaceV1Private(this))
{
}

XdgDialogManagerInterfaceV1::~XdgDialogManagerInterfaceV1() = default;

QByteArrayView XdgDialogManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void XdgDialogManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void XdgDialogManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *XdgDialogManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
