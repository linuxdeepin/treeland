// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activationtokenmanagerinterfacev1.h"

#include "common/treelandlogging.h"
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
    explicit ActivationTokenManagerInterfaceV1Private(ActivationManagerInterfaceV1 *activation)
        : QtWaylandServer::treeland_activation_token_manager_v1()
        , m_activation(activation)
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

protected:
    void destroy_global() override;
    void destroy(Resource *resource) override;
    void request_token(Resource *resource, uint32_t id) override;

private:
    QPointer<ActivationManagerInterfaceV1> m_activation;
};

void ActivationTokenManagerInterfaceV1Private::destroy_global()
{
    qCDebug(lcTlActivation) << "ActivationTokenManagerInterfaceV1 global destroyed";
}

void ActivationTokenManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ActivationTokenManagerInterfaceV1Private::request_token(Resource *resource, uint32_t id)
{
    if (!m_activation) {
        qCWarning(lcTlActivation)
            << "request_token rejected: activation protocol is not available for client"
            << resource->client();
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
    token->send_token(tokenResource, m_activation->mintToken());
    qCDebug(lcTlActivation) << "Minted privileged activation token for client"
                            << resource->client();
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
    d->init(server->handle(), InterfaceVersion);
    qCDebug(lcTlActivation) << "ActivationTokenManagerInterfaceV1 global created";
}

void ActivationTokenManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
    qCDebug(lcTlActivation) << "ActivationTokenManagerInterfaceV1 global removal scheduled";
}

wl_global *ActivationTokenManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
