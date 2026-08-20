// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "activationmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "qwayland-server-xdg-activation-v1.h"

#include <wserver.h>
#include <wseat.h>
#include <wsurface.h>

#include <QDeadlineTimer>
#include <QPointer>
#include <QUuid>

#include <algorithm>
#include <optional>

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

    struct ::wl_resource *tokenResourceHandle() const { return resource()->handle; }

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
                    struct ::wl_resource *seat) override
    {
        if (m_committed)
            return;

        auto *seatClient = seat ? wlr_seat_client_from_resource(seat) : nullptr;
        if (!seatClient) {
            qCWarning(lcTlActivation) << "set_serial: invalid seat resource, ignoring serial" << serial;
            m_serial.reset();
            m_seat.clear();
            return;
        }

        auto *wseat = WSeat::fromHandle(seatClient->seat);
        if (!wseat) {
            qCWarning(lcTlActivation) << "set_serial: no WSeat for seat resource, ignoring serial" << serial;
            m_serial.reset();
            m_seat.clear();
            return;
        }

        m_serial = serial;
        m_seat = wseat;
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
    QPointer<WSeat> m_seat; // set by set_serial; null if not called or seat destroyed
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
    QPointer<WSeat> seat;            // seat associated with the token via set_serial
    QPointer<WSurface> originatingSurface; // surface set via set_surface
    QRectF launchRect;                     // local rect relative to originatingSurface (invalid if none)
};

class ActivationManagerInterfaceV1Private
    : public QtWaylandServer::xdg_activation_v1
{
public:
    explicit ActivationManagerInterfaceV1Private(ActivationManagerInterfaceV1 *q,
                                                 std::function<bool(WSurface *, WSeat *)> trustedSurfaceChecker)
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
                          bool fromTrustedSurface,
                          WSeat *seat,
                          WSurface *originatingSurface,
                          const QRectF &launchRect)
    {
        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_tokens.append(TokenInfo{ token, appId, client, serial, fromTrustedSurface,
                                   QDeadlineTimer(TokenLifetimeMs),
                                   seat, originatingSurface, launchRect });
        qCDebug(lcTlActivation) << "Registered activation token" << token.left(8) + u"..."_s
                               << "for app" << appId
                               << (fromTrustedSurface ? "" : "(inactive-surface-token-request)")
                               << (launchRect.isValid() ? "with launch rect" : "");
        return token;
    }

    bool isTrustedSurface(WSurface *surface, WSeat *seat) const
    {
        if (m_trustedSurfaceChecker && seat)
            return m_trustedSurfaceChecker(surface, seat);
        return false; // conservative: no checker → treat as inactive
    }

    ActivationManagerInterfaceV1::LaunchRectProvider launchRectProvider;

protected:
    void destroy_global() override
    {
        qCDebug(lcTlActivation) << "treeland_activation_manager_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_activation_token(Resource *resource, uint32_t id) override
    {
        auto *tokenResource = wl_resource_create(resource->client(),
                                                  &xdg_activation_token_v1_interface,
                                                  resource->version(),
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
            qCWarning(lcTlActivation) << "activate: invalid surface resource";
            return;
        }

        auto *wsurface = WSurface::fromHandle(wlrSurface);
        if (!wsurface) {
            qCWarning(lcTlActivation) << "activate: no WSurface for wlr_surface";
            return;
        }

        // Single lookup: resolve disposition, seat, originating surface and
        // launch rect from the same token iterator (avoids a second linear scan).
        ActivationManagerInterfaceV1::TokenDisposition disposition =
            ActivationManagerInterfaceV1::TokenDisposition::Invalid;
        WSeat *tokenSeat = nullptr;
        WSurface *originatingSurface = nullptr;
        QRectF launchRect;

        auto it = std::find_if(m_tokens.begin(), m_tokens.end(),
                               [&token](const TokenInfo &t) { return t.token == token; });
        if (it != m_tokens.end()) {
            tokenSeat = it->seat.data();
            originatingSurface = it->originatingSurface;
            if (!it->expiry.hasExpired()) {
                // Only carry forward the launch rect for non-expired tokens.
                // An expired token's rect is stale and must not trigger an
                // animation on a surface that maps later.
                launchRect = it->launchRect;
                // inactive-surface-token-request: set_surface not called or surface was not active
                // → treat as Attention
                disposition = it->fromTrustedSurface
                                  ? (it->serial.has_value()
                                         ? ActivationManagerInterfaceV1::TokenDisposition::Active
                                         : ActivationManagerInterfaceV1::TokenDisposition::Attention)
                                  : ActivationManagerInterfaceV1::TokenDisposition::Attention;
            }
        }

        // If the token has no originating surface, the launch rect cannot be
        // mapped to global coordinates — clear it to prevent an orphan rect
        // from being forwarded downstream.
        if (!originatingSurface)
            launchRect = QRectF();

        // Allow activation of unmapped surfaces when a launch rect is present;
        // the compositor will play the animation when the surface maps.
        if (!wsurface->mapped() && !launchRect.isValid()) {
            qCWarning(lcTlActivation) << "activate: unmapped surface without launch rect";
            // Keep token one-shot semantics: consume the token even on early return
            if (it != m_tokens.end())
                m_tokens.erase(it);
            return;
        }

        qCInfo(lcTlActivation) << "activate: emitting activateRequested for token" << token.left(8) + u"..."_s
                             << "with disposition" << disposition
                             << (launchRect.isValid() ? "with launch rect" : "");

        // Keep token one-shot semantics
        if (it != m_tokens.end()) {
            m_tokens.erase(it);
        }

        Q_EMIT q->activateRequested(disposition, wsurface, tokenSeat, originatingSurface, launchRect);
    }

private:
    void sweepExpiredTokens()
    {
        auto it = m_tokens.begin();
        while (it != m_tokens.end()) {
            if (it->expiry.hasExpired()) {
                qCDebug(lcTlActivation) << "Sweeping expired token for app" << it->appId;
                it = m_tokens.erase(it);
            } else {
                ++it;
            }
        }
    }

    ActivationManagerInterfaceV1 *q;
    QList<TokenInfo> m_tokens;
    std::function<bool(WSurface *, WSeat *)> m_trustedSurfaceChecker;

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
    const bool fromTrustedSurface = m_surface && m_manager->isTrustedSurface(m_surface, m_seat);

    // Query the window animation manager for a committed rect on this token resource.
    // Only attach a launch rect when an originating surface exists; a rect without
    // an origin cannot be converted to global coordinates and would be an orphan.
    QRectF launchRect;
    if (m_surface && m_manager->launchRectProvider) {
        auto rectOpt = m_manager->launchRectProvider(tokenResourceHandle());
        if (rectOpt)
            launchRect = *rectOpt;
    }

    const QString token = m_manager->registerToken(m_appId, resource->client(), m_serial,
                                                   fromTrustedSurface, m_seat, m_surface, launchRect);
    send_done(token);
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

ActivationManagerInterfaceV1::ActivationManagerInterfaceV1(
    std::function<bool(WAYLIB_SERVER_NAMESPACE::WSurface *, WAYLIB_SERVER_NAMESPACE::WSeat *)> trustedSurfaceChecker,
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

void ActivationManagerInterfaceV1::setLaunchRectProvider(LaunchRectProvider provider)
{
    d->launchRectProvider = std::move(provider);
}

void ActivationManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void ActivationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ActivationManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
