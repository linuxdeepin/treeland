// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activationtokenmanagerinterfacev1.h"

#include "modules/activation/activationmanagerinterfacev1.h"
#include "qwayland-server-treeland-activation-token-unstable-v1.h"

#include <wserver.h>

#include <QPointer>

WAYLIB_SERVER_USE_NAMESPACE

// ---------------------------------------------------------------------------
// Token object
// ---------------------------------------------------------------------------

class ActivationTokenV1 : public QtWaylandServer::treeland_activation_token_v1
{
public:
    explicit ActivationTokenV1(struct ::wl_resource *resource)
        : QtWaylandServer::treeland_activation_token_v1(resource)
    {
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override
    {
        delete this;
    }
};

// ---------------------------------------------------------------------------
// Manager private
// ---------------------------------------------------------------------------

class ActivationTokenManagerInterfaceV1Private
    : public QtWaylandServer::treeland_activation_token_manager_v1
{
public:
    ActivationTokenManagerInterfaceV1Private(ActivationManagerInterfaceV1 *activation)
        : m_activation(activation)
    {
    }

    wl_global *globalHandle() const
    {
        return m_customGlobal;
    }

    // The generated bind is private and cannot reject, but a second concurrent
    // bind must be answered with binding_rejected instead of simply being
    // invisible, so the global is created by hand.
    void start(wl_display *display, uint32_t version)
    {
        m_customGlobal = wl_global_create(display, interface(), version, this, bindCallback);
    }

    void stop()
    {
        if (!m_customGlobal)
            return;
        wl_global_destroy(m_customGlobal);
        m_customGlobal = nullptr;
    }

protected:
    void destroy(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void request_token(Resource *resource, uint32_t id) override;

private:
    static void bindCallback(wl_client *client, void *data, uint32_t version, uint32_t id);
    void bindClient(wl_client *client, uint32_t id, uint32_t version);
    void rejectBind(wl_client *client, uint32_t id, uint32_t version, uint32_t error,
                    const char *message);

    QPointer<ActivationManagerInterfaceV1> m_activation;
    // Owned here instead of in the generated base: the base only destroys its
    // m_global together with a display listener this class never installs.
    wl_global *m_customGlobal = nullptr;
    wl_client *m_boundClient = nullptr;
};

void ActivationTokenManagerInterfaceV1Private::bindCallback(wl_client *client,
                                                           void *data,
                                                           uint32_t version,
                                                           uint32_t id)
{
    static_cast<ActivationTokenManagerInterfaceV1Private *>(data)->bindClient(client, id, version);
}

void ActivationTokenManagerInterfaceV1Private::bindClient(wl_client *client,
                                                          uint32_t id,
                                                          uint32_t version)
{
    // Singleton: the manager is per-Session and at most one client may bind it
    // at a time. A second client is rejected with binding_rejected. The caller
    // is not otherwise verified yet; see the TODO in the header about
    // appid-based authentication.
    if (m_boundClient) {
        rejectBind(client,
                   id,
                   version,
                   TREELAND_ACTIVATION_TOKEN_MANAGER_V1_ERROR_BINDING_REJECTED,
                   "the activation token manager is already bound by another client");
        return;
    }

    m_boundClient = client;
    add(client, id, version);
}

void ActivationTokenManagerInterfaceV1Private::rejectBind(wl_client *client,
                                                          uint32_t id,
                                                          uint32_t version,
                                                          uint32_t error,
                                                          const char *message)
{
    // An error can only travel on a resource, so create the object for it and
    // destroy it again: a rejected bind leaves neither state nor a usable object.
    auto *resource = wl_resource_create(client, interface(), version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_post_error(resource, error, "%s", message);
    wl_resource_destroy(resource);
}

void ActivationTokenManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ActivationTokenManagerInterfaceV1Private::destroy_resource(Resource *resource)
{
    // Covers both the destroy request and client crash/disconnect.
    if (resource->client() == m_boundClient)
        m_boundClient = nullptr;
}

void ActivationTokenManagerInterfaceV1Private::request_token(Resource *resource, uint32_t id)
{
    if (!m_activation) {
        wl_resource_post_error(resource->handle,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "activation protocol is not available");
        return;
    }

    auto *tokenResource = wl_resource_create(resource->client(),
                                             &treeland_activation_token_v1_interface,
                                             resource->version(),
                                             id);
    if (!tokenResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    // ActivationTokenV1 self-destructs in destroy_resource()
    auto *token = new ActivationTokenV1(tokenResource);
    // The protocol carries no app id any more; app_id is only used for logging.
    token->send_token(tokenResource, m_activation->mintToken(QString(), nullptr));
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

ActivationTokenManagerInterfaceV1::ActivationTokenManagerInterfaceV1(
    ActivationManagerInterfaceV1 *activation, QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ActivationTokenManagerInterfaceV1Private(activation))
{
}

ActivationTokenManagerInterfaceV1::~ActivationTokenManagerInterfaceV1() = default;

QByteArrayView ActivationTokenManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void ActivationTokenManagerInterfaceV1::create(WServer *server)
{
    d->start(server->handle(), InterfaceVersion);
}

void ActivationTokenManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->stop();
}

wl_global *ActivationTokenManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
