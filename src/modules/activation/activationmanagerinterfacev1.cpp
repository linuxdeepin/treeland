// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activationmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "qwayland-server-xdg-activation-v1.h"

#include <wserver.h>
#include <wsurface.h>

#include <qwdisplay.h>

#include <QDeadlineTimer>
#include <QPointer>
#include <QUuid>

#include <algorithm>
#include <optional>

extern "C" {
#include <wlr/types/wlr_compositor.h>
}

WAYLIB_SERVER_USE_NAMESPACE
using namespace Qt::StringLiterals;

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------

class ActivationManagerInterfaceV1Private;

// ---------------------------------------------------------------------------
// Token context — manages one in-progress token object
// ---------------------------------------------------------------------------

/**
 * Represents a single xdg_activation_token_v1 resource.
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
        // TODO: validate serial/seat pair and support multi-seat
        if (m_committed)
            return;
        m_serial = serial;
    }

    void set_surface(Resource * /*resource*/,
                     struct ::wl_resource *surface) override;

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
    std::optional<uint32_t> m_serial;
    bool m_committed = false;
    QPointer<WSurface> m_surface; // set by set_surface; null if not called or surface destroyed
};

// ---------------------------------------------------------------------------
// Manager private
// ---------------------------------------------------------------------------

struct TokenInfo
{
    QString token;
    QString appId;
    wl_client *requestingClient = nullptr;
    std::optional<uint32_t> serial;
    bool fromTrustedSurface = false; // set_surface called and surface was active at commit time
    QDeadlineTimer expiry;           // invalidated 60 s after registration
};

class ActivationManagerInterfaceV1Private
    : public QtWaylandServer::xdg_activation_v1
{
public:
    explicit ActivationManagerInterfaceV1Private(ActivationManagerInterfaceV1 *q,
                                                 std::function<bool(WSurface *)> trustedSurfaceChecker)
        : QtWaylandServer::xdg_activation_v1()
        , q(q)
        , m_trustedSurfaceChecker(std::move(trustedSurfaceChecker))
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
    QString registerToken(const QString &appId,
                          wl_client *client,
                          std::optional<uint32_t> serial,
                          bool fromTrustedSurface)
    {
        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_tokens.append(TokenInfo{ token, appId, client, serial, fromTrustedSurface,
                                   QDeadlineTimer(TokenLifetimeMs) });
        qCDebug(treelandActivation) << "Registered activation token" << token.left(8) + u"..."_s
                               << "for app" << appId
                               << (fromTrustedSurface ? "" : "(inactive-surface-token-request)");
        return token;
    }

    bool isTrustedSurface(WSurface *surface) const
    {
        if (m_trustedSurfaceChecker)
            return m_trustedSurfaceChecker(surface);
        return false; // conservative: no checker → treat as inactive
    }

protected:
    void destroy_global() override
    {
        qCDebug(treelandActivation) << "treeland_activation_manager_v1 global destroyed";
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
        sweepExpiredTokens();

        auto *wlrSurface = wlr_surface_from_resource(surface);
        if (!wlrSurface) {
            qCWarning(treelandActivation) << "activate: invalid surface resource";
            return;
        }

        auto *wsurface = WSurface::fromHandle(wlrSurface);
        if (!wsurface) {
            qCWarning(treelandActivation) << "activate: no WSurface for wlr_surface";
            return;
        }

        auto disposition = dispositionForToken(token);
        qCInfo(treelandActivation) << "activate: emitting activateRequested for token" << token.left(8) + u"..."_s
                             << "with disposition" << disposition;
        // Keep token one-shot semantics; Helper performs the policy check.
        auto it = std::find_if(m_tokens.begin(), m_tokens.end(),
                               [&token](const TokenInfo &t) { return t.token == token; });
        if (it != m_tokens.end()) {
            m_tokens.erase(it);
        }
        
        Q_EMIT q->activateRequested(disposition, wsurface);
    }

private:
    ActivationManagerInterfaceV1::TokenDisposition dispositionForToken(const QString &token) const
    {
        auto it = std::find_if(m_tokens.cbegin(), m_tokens.cend(),
                               [&token](const TokenInfo &t) { return t.token == token; });
        if (it == m_tokens.cend()) {
            return ActivationManagerInterfaceV1::TokenDisposition::Invalid;
        }
        if (it->expiry.hasExpired()) {
            return ActivationManagerInterfaceV1::TokenDisposition::Invalid;
        }
        // inactive-surface-token-request: set_surface not called or surface was not active
        // → treat as Attention
        if (!it->fromTrustedSurface) {
            return ActivationManagerInterfaceV1::TokenDisposition::Attention;
        }
        return it->serial.has_value() ? ActivationManagerInterfaceV1::TokenDisposition::Active
                                      : ActivationManagerInterfaceV1::TokenDisposition::Attention;
    }

    void sweepExpiredTokens()
    {
        auto it = m_tokens.begin();
        while (it != m_tokens.end()) {
            if (it->expiry.hasExpired()) {
                qCDebug(treelandActivation) << "Sweeping expired token for app" << it->appId;
                it = m_tokens.erase(it);
            } else {
                ++it;
            }
        }
    }

    ActivationManagerInterfaceV1 *q;
    QList<TokenInfo> m_tokens;
    std::function<bool(WSurface *)> m_trustedSurfaceChecker;

    static constexpr int TokenLifetimeMs = 60'000;
};

// ---------------------------------------------------------------------------
// TokenContext methods — defined after ActivationManagerInterfaceV1Private
// ---------------------------------------------------------------------------

void TokenContext::set_surface(Resource * /*resource*/, struct ::wl_resource *surface)
{
    if (m_committed)
        return;
    auto *wlrSurface = wlr_surface_from_resource(surface);
    m_surface = wlrSurface ? WSurface::fromHandle(wlrSurface) : nullptr;
}

void TokenContext::commit(Resource *resource)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle,
                               error_already_used,
                               "commit() called a second time on this token");
        return;
    }
    m_committed = true;

    // fromTrustedSurface: set_surface was called, surface still alive, and currently active
    const bool fromTrustedSurface = m_surface && m_manager->isTrustedSurface(m_surface);
    const QString token = m_manager->registerToken(m_appId, resource->client(), m_serial, fromTrustedSurface);
    send_done(token);
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

ActivationManagerInterfaceV1::ActivationManagerInterfaceV1(
    std::function<bool(WSurface *)> trustedSurfaceChecker,
    QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ActivationManagerInterfaceV1Private(this, std::move(trustedSurfaceChecker)))
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



