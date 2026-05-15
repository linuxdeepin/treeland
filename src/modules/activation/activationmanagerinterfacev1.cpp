// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activationmanagerinterfacev1.h"
#include "qwayland-server-xdg-activation-v1.h"

#include <wserver.h>
#include <wsurface.h>

#include <qwdisplay.h>

#include <QHash>
#include <QLoggingCategory>
#include <QUuid>

extern "C" {
#include <wlr/types/wlr_compositor.h>
}

WAYLIB_SERVER_USE_NAMESPACE

Q_LOGGING_CATEGORY(lcActivation, "treeland.activation", QtInfoMsg)

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------

class ActivationManagerInterfaceV1Private;

// ---------------------------------------------------------------------------
// Token context — manages one in-progress token object
// ---------------------------------------------------------------------------

/**
 * Represents a single treeland_activation_token_v1 resource.
 *
 * The client calls set_serial / set_surface / set_app_id (all optional) and
 * then commit() to obtain the token string.  After commit() the object is
 * inert.  The object self-destructs via destroy_resource().
 */
class TokenContext : public QtWaylandServer::xdg_activation_token_v1
{
public:
    TokenContext(ActivationManagerInterfaceV1Private *manager,
                 struct ::wl_resource *resource)
        : QtWaylandServer::xdg_activation_token_v1(resource)
        , m_manager(manager)
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

    void set_serial(Resource * /*resource*/,
                    uint32_t serial,
                    struct ::wl_resource * /*seat*/) override
    {
        if (m_committed)
            return;
        m_serial = serial;
    }

    void set_surface(Resource * /*resource*/,
                     struct ::wl_resource * /*surface*/) override
    {
        // Stored for future policy use; currently unused in v1.
        if (m_committed)
            return;
    }

    void set_app_id(Resource * /*resource*/, const QString &app_id) override
    {
        if (m_committed)
            return;
        m_appId = app_id;
    }

    void commit(Resource *resource) override;

private:
    ActivationManagerInterfaceV1Private *m_manager;
    QString m_appId;
    uint32_t m_serial = 0;
    bool m_committed = false;
};

// ---------------------------------------------------------------------------
// Manager private
// ---------------------------------------------------------------------------

struct TokenInfo
{
    QString appId;
    wl_client *requestingClient = nullptr;
};

class ActivationManagerInterfaceV1Private
    : public QtWaylandServer::xdg_activation_v1
{
public:
    explicit ActivationManagerInterfaceV1Private(ActivationManagerInterfaceV1 *q)
        : QtWaylandServer::xdg_activation_v1()
        , q(q)
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    /**
     * Called by TokenContext::commit() to register a new valid token.
     * Returns the generated token string.
     */
    QString registerToken(const QString &appId, wl_client *client)
    {
        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_tokens.insert(token, TokenInfo{ appId, client });
        qCDebug(lcActivation) << "Registered activation token" << token
                               << "for app" << appId;
        return token;
    }

protected:
    void destroy_global() override
    {
        qCDebug(lcActivation) << "treeland_activation_manager_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_activation_token(Resource *resource, uint32_t id) override
    {
        auto *tokenResource = wl_resource_create(resource->client(),
                                                  &xdg_activation_token_v1_interface,
                                                  wl_resource_get_version(resource->handle),
                                                  id);
        if (!tokenResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        // TokenContext self-destructs in destroy_resource()
        new TokenContext(this, tokenResource);
    }

    void activate(Resource * /*resource*/,
                  const QString &token,
                  struct ::wl_resource *surface) override
    {
        // v1 permissive check: accept any committed token that is in our map.
        auto it = m_tokens.find(token);
        if (it == m_tokens.end()) {
            qCDebug(lcActivation) << "activate: unknown token" << token << "— ignored";
            return;
        }

        m_tokens.erase(it); // consume the token

        auto *wlrSurface = wlr_surface_from_resource(surface);
        if (!wlrSurface) {
            qCWarning(lcActivation) << "activate: invalid surface resource";
            return;
        }

        auto *wsurface = WSurface::fromHandle(wlrSurface);
        if (!wsurface) {
            qCWarning(lcActivation) << "activate: no WSurface for wlr_surface";
            return;
        }

        qCInfo(lcActivation) << "activate: emitting activateRequested for token" << token;
        Q_EMIT q->activateRequested(token, wsurface);
    }

private:
    ActivationManagerInterfaceV1 *q;
    QHash<QString, TokenInfo> m_tokens;
};

// ---------------------------------------------------------------------------
// TokenContext::commit — defined after ActivationManagerInterfaceV1Private
// ---------------------------------------------------------------------------

void TokenContext::commit(Resource *resource)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle,
                               error_already_used,
                               "commit() called a second time on this token");
        return;
    }
    m_committed = true;

    const QString token = m_manager->registerToken(m_appId, resource->client());
    send_done(token);
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

ActivationManagerInterfaceV1::ActivationManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ActivationManagerInterfaceV1Private(this))
{
}

ActivationManagerInterfaceV1::~ActivationManagerInterfaceV1() = default;

QByteArrayView ActivationManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void ActivationManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void ActivationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ActivationManagerInterfaceV1::global() const
{
    return d->globalHandle();
}

