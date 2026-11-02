// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionprotocols.h"
#include "session.h"
#include <wserver.h>
#include <wxdgshell.h>
#include <wlayershell.h>
#include <wxdgdecorationmanager.h>
#include <wxdgdialogmanagerv1.h>
#include <wxdgtopleveltagmanager.h>
#include <wlr_all.h>
#include <type_traits>
#include <memory>
#include <utility>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
constexpr uint32_t ExtDataControlManagerVersion = 1;

// Native managers are display-owned. Session withdraws every advertised global
// before releasing the wrapper; client resources are destroyed before detach.
template<auto Create>
class NativeSessionProtocol : public QObject, public WServerInterface
{
    using Native = std::remove_pointer_t<std::invoke_result_t<decltype(Create), wl_display *>>;
public:
    explicit NativeSessionProtocol(QByteArrayView name) : m_name(name) {}
    QByteArrayView interfaceName() const override { return m_name; }
protected:
    void create(WServer *server) override { m_handle = Create(server->handle()); }
    void destroy(WServer *) override { m_handle = nullptr; }
    wl_global *global() const override
    {
        auto *native = nativeInterface<Native>();
        return native ? native->global : nullptr;
    }
private:
    const QByteArrayView m_name;
};

wlr_ext_data_control_manager_v1 *createExtDataControl(wl_display *display)
{
    return wlr_ext_data_control_manager_v1_create(display, ExtDataControlManagerVersion);
}

// The native factory creates both globals together. Each advertised interface
// has one wrapper; export/import tokens share only this Session's registry.
struct SessionForeignRegistry
{
    wlr_xdg_foreign_v2 *acquire(WServer *server)
    {
        if (users++ == 0) {
            auto *registry = wlr_xdg_foreign_registry_create(server->handle());
            if (!registry)
                return nullptr;
            native = wlr_xdg_foreign_v2_create(server->handle(), registry);
        }
        return native;
    }

    void release()
    {
        if (--users == 0)
            native = nullptr; // Native state is reclaimed by wl_display teardown.
    }

    wlr_xdg_foreign_v2 *native = nullptr;
    int users = 0;
};

class SessionForeignInterface final : public QObject, public WServerInterface
{
public:
    enum Endpoint { Exporter, Importer };
    SessionForeignInterface(std::shared_ptr<SessionForeignRegistry> registry, Endpoint endpoint)
        : m_registry(std::move(registry))
        , m_endpoint(endpoint)
    {}
    QByteArrayView interfaceName() const override
    {
        return m_endpoint == Exporter ? "zxdg_exporter_v2" : "zxdg_importer_v2";
    }
protected:
    void create(WServer *server) override { m_handle = m_registry->acquire(server); }
    void destroy(WServer *) override
    {
        m_registry->release();
        m_handle = nullptr;
    }
    wl_global *global() const override
    {
        auto *native = nativeInterface<wlr_xdg_foreign_v2>();
        if (!native)
            return nullptr;
        return m_endpoint == Exporter ? native->exporter.global : native->importer.global;
    }
private:
    const std::shared_ptr<SessionForeignRegistry> m_registry;
    const Endpoint m_endpoint;
};
}

SessionProtocols::SessionProtocols(Session *session)
    : SessionComponent(session)
{
    session->attachInterface(new NativeSessionProtocol<wlr_data_device_manager_create>("wl_data_device_manager"));
    session->attachInterface(new NativeSessionProtocol<wlr_primary_selection_v1_device_manager_create>("zwp_primary_selection_device_manager_v1"));
    session->attachInterface(new NativeSessionProtocol<wlr_data_control_manager_v1_create>("zwlr_data_control_manager_v1"));
    session->attachInterface(new NativeSessionProtocol<createExtDataControl>("ext_data_control_manager_v1"));
    auto foreignRegistry = std::make_shared<SessionForeignRegistry>();
    session->attachInterface(new SessionForeignInterface(foreignRegistry, SessionForeignInterface::Exporter));
    session->attachInterface(new SessionForeignInterface(foreignRegistry, SessionForeignInterface::Importer));
    m_xdgShell = new WXdgShell(5);
    session->attachInterface(m_xdgShell);
    m_layerShell = new WLayerShell(m_xdgShell);
    session->attachInterface(m_layerShell);
    m_xdgDecoration = new WXdgDecorationManager;
    session->attachInterface(m_xdgDecoration);
    m_xdgDialog = new WXdgDialogManagerV1;
    session->attachInterface(m_xdgDialog);
    m_xdgTags = new WXdgToplevelTagManagerV1;
    session->attachInterface(m_xdgTags);
}

WXdgShell *SessionProtocols::xdgShell() const { return m_xdgShell; }
WLayerShell *SessionProtocols::layerShell() const { return m_layerShell; }
WXdgDecorationManager *SessionProtocols::xdgDecoration() const { return m_xdgDecoration; }
WXdgDialogManagerV1 *SessionProtocols::xdgDialog() const { return m_xdgDialog; }
WXdgToplevelTagManagerV1 *SessionProtocols::xdgTags() const { return m_xdgTags; }
