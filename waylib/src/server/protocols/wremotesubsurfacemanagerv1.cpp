// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wremotesubsurfacemanagerv1.h"

#include "private/wglobal_p.h"
#include "qwayland-server-treeland-remote-subsurface-unstable-v1.h"
#include "wayliblogging.h"
#include "wserver.h"
#include "wscoplistener.h"
#include "wsubsurface.h"
#include "wsurface.h"

extern "C" {
#include <wlr/types/wlr_compositor.h>
}

#include <QHash>
#include <QPointer>
#include <QUuid>

WAYLIB_SERVER_BEGIN_NAMESPACE

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

class WRemoteSubsurfaceManagerV1Private;
class ExportedSurfaceContext;
class RemoteSubsurfaceContext;

// ---------------------------------------------------------------------------
// wlroots role for remote subsurfaces (defined after RemoteSubsurfaceContext)
// ---------------------------------------------------------------------------

static void remote_subsurface_role_commit(struct wlr_surface *surface);
static void remote_subsurface_role_destroy(struct wlr_surface *surface);
static const wlr_surface_role remoteSubsurfaceRole = {
    .name = "treeland_remote_subsurface_v1",
    .no_object = false,
    .client_commit = nullptr,
    .commit = remote_subsurface_role_commit,
    .map = nullptr,
    .unmap = nullptr,
    .destroy = remote_subsurface_role_destroy,
};

// ---------------------------------------------------------------------------
// ExportedSurfaceContext — one per exported wl_surface
// ---------------------------------------------------------------------------

class ExportedSurfaceContext
    : public QObject
    , public QtWaylandServer::treeland_exported_surface_v1
{
    Q_OBJECT
public:
    ExportedSurfaceContext(WRemoteSubsurfaceManagerV1Private *manager,
                           wlr_surface *surface,
                           const QString &token,
                           wl_client *client,
                           uint32_t version,
                           uint32_t id)
        : QObject()
        , QtWaylandServer::treeland_exported_surface_v1(client, id, version)
        , m_manager(manager)
        , m_surface(surface)
        , m_token(token)
    {
        // WSurface follows a surface role and may be destroyed with an
        // xdg_toplevel while the underlying wl_surface remains valid.
        m_surfaceDestroyListener.init(&m_surface->events.destroy,
                                      this,
                                      &ExportedSurfaceContext::handleSurfaceDestroy);
    }

    ~ExportedSurfaceContext() override = default;

    wlr_surface *surface() const
    {
        return m_surface;
    }


    QString token() const
    {
        return m_token;
    }

    // Back-pointer to the RemoteSubsurfaceContext where this context acts as child.
    // Set by addChildToParent / removeChildFromParent in the manager.
    RemoteSubsurfaceContext *remoteSubsurface = nullptr;

    // Remote children ordered relative to this exported parent surface.
    QList<RemoteSubsurfaceContext *> belowChildren;
    QList<RemoteSubsurfaceContext *> aboveChildren;

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override;
    void create_remote_subsurface(Resource *resource,
                                  uint32_t id,
                                  const QString &parent_token) override;

private:
    void handleSurfaceDestroy();

    WRemoteSubsurfaceManagerV1Private *m_manager;
    wlr_surface *m_surface = nullptr;
    QString m_token;
    WScopedListener m_surfaceDestroyListener;
};

// ---------------------------------------------------------------------------
// RemoteSubsurfaceContext — one per parent-child relationship
// ---------------------------------------------------------------------------

class RemoteSubsurfaceContext
    : public QObject
    , public QtWaylandServer::treeland_remote_subsurface_v1
{
    Q_OBJECT
public:
    RemoteSubsurfaceContext(WRemoteSubsurfaceManagerV1Private *manager,
                            ExportedSurfaceContext *childExported,
                            ExportedSurfaceContext *parentExported,
                            wl_client *client,
                            uint32_t version,
                            uint32_t id)
        : QObject()
        , QtWaylandServer::treeland_remote_subsurface_v1(client, id, version)
        , m_manager(manager)
        , m_child(childExported)
        , m_parent(parentExported)
    {
    }

    ~RemoteSubsurfaceContext() override;

    ExportedSurfaceContext *childExported() const
    {
        return m_child;
    }

    ExportedSurfaceContext *parentExported() const
    {
        return m_parent;
    }

    WSubsurface *subsurface() const { return m_subsurface; }
    void setSubsurface(WSubsurface *s) { m_subsurface = s; }
    QPointF position() const { return m_position; }

    void invalidate() { m_manager = nullptr; }

    void ensureSubsurface();

    void recheckMapping();
    void cascadeUnmap();

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override;
    void set_position(Resource *resource, int32_t x, int32_t y) override;
    void place_above(Resource *resource, const QString &sibling_token) override;
    void place_below(Resource *resource, const QString &sibling_token) override;

private:
    WRemoteSubsurfaceManagerV1Private *m_manager;
    ExportedSurfaceContext *m_child = nullptr;
    ExportedSurfaceContext *m_parent = nullptr;
    QPointer<WSubsurface> m_subsurface;
    QPointF m_position;
};

// ---------------------------------------------------------------------------
// WRemoteSubsurfaceManagerV1Private
// ---------------------------------------------------------------------------

static inline WSurface *wsurfaceFrom(const ExportedSurfaceContext *ctx)
{
    auto *wlr = ctx ? ctx->surface() : nullptr;
    return wlr ? WSurface::fromHandle(wlr) : nullptr;
}

static inline QStringView shortToken(const QString &token)
{
    return QStringView(token).left(8);
}

class WRemoteSubsurfaceManagerV1Private
    : public WObjectPrivate
    , public QtWaylandServer::treeland_remote_subsurface_manager_v1
{
    Q_DECLARE_PUBLIC(WRemoteSubsurfaceManagerV1)

public:
    explicit WRemoteSubsurfaceManagerV1Private(WRemoteSubsurfaceManagerV1 *q)
        : WObjectPrivate(q)
        , QtWaylandServer::treeland_remote_subsurface_manager_v1()
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    // Lookup
    ExportedSurfaceContext *findExportedByToken(const QString &token) const
    {
        return m_tokens.value(token, nullptr);
    }

    bool isSurfaceExported(wlr_surface *wlrSurface) const
    {
        if (!wlrSurface)
            return false;
        for (auto it = m_tokens.constBegin(); it != m_tokens.constEnd(); ++it) {
            if (it.value()->surface() == wlrSurface)
                return true;
        }
        return false;
    }

    // Registry
    void registerExported(ExportedSurfaceContext *ctx)
    {
        m_tokens.insert(ctx->token(), ctx);
    }

    void unregisterExported(ExportedSurfaceContext *ctx)
    {
        m_tokens.remove(ctx->token());
    }

    // Cycle detection
    bool isDescendantOf(ExportedSurfaceContext *candidate, ExportedSurfaceContext *root) const
    {
        if (!candidate || !root)
            return false;
        auto *curr = candidate;
        while (curr) {
            if (curr == root)
                return true;
            curr = curr->remoteSubsurface ? curr->remoteSubsurface->parentExported() : nullptr;
        }
        return false;
    }

    // WSurface::addRemoteSubsurface() is private; this manager is a friend of
    // WSurface, so expose a static wrapper so RemoteSubsurfaceContext can call it.
    static WSubsurface *createRemoteSubsurface(WSurface *parent, wlr_surface *child)
    {
        return parent->addRemoteSubsurface(child);
    }

    // Children list management (z-order, below vs above parent)
    QList<RemoteSubsurfaceContext *> childrenOf(ExportedSurfaceContext *parent) const
    {
        if (!parent)
            return {};
        QList<RemoteSubsurfaceContext *> res = parent->belowChildren;
        res.append(parent->aboveChildren);
        return res;
    }

    void addChildToParent(RemoteSubsurfaceContext *remote)
    {
        auto *parent = remote->parentExported();
        parent->aboveChildren.append(remote);
        remote->childExported()->remoteSubsurface = remote;

        auto *parentSurface = wsurfaceFrom(parent);
        auto *childWlr = remote->childExported()->surface();
        if (!parentSurface || !childWlr)
            return;

        // WSubsurface is destroyed together with the parent's WSurface.
        // WSurface lifetime follows its surface role (remote_subsurface,
        // xdg_toplevel, etc.). Recreating a remote parent's role requires
        // recreating WSubsurface for its children.
        remote->setSubsurface(parentSurface->addRemoteSubsurface(childWlr));
    }

    void removeChildFromOrder(RemoteSubsurfaceContext *remote)
    {
        auto *parent = remote->parentExported();
        if (!parent)
            return;
        parent->belowChildren.removeOne(remote);
        parent->aboveChildren.removeOne(remote);
    }

    void removeChildFromParent(RemoteSubsurfaceContext *remote)
    {
        removeChildFromOrder(remote);

        auto *child = remote->childExported();
        if (child && child->remoteSubsurface == remote)
            child->remoteSubsurface = nullptr;

        if (auto *subsurface = remote->subsurface()) {
            if (auto *parentSurface = subsurface->parentSurface())
                parentSurface->removeSubsurface(subsurface);
            remote->setSubsurface(nullptr);
        }
    }

    void syncRemoteSubsurfaceOrder(ExportedSurfaceContext *parent)
    {
        auto *parentSurface = wsurfaceFrom(parent);
        if (!parentSurface)
            return;

        QList<WSubsurface *> below;
        QList<WSubsurface *> above;
        for (auto *remote : parent->belowChildren) {
            if (auto *subsurface = remote->subsurface())
                below.append(subsurface);
        }
        for (auto *remote : parent->aboveChildren) {
            if (auto *subsurface = remote->subsurface())
                above.append(subsurface);
        }
        parentSurface->setRemoteSubsurfaceOrder(below, above);
    }

    void placeChildAboveParentTop(RemoteSubsurfaceContext *remote)
    {
        auto *parent = remote->parentExported();
        removeChildFromOrder(remote);
        parent->aboveChildren.append(remote);
        syncRemoteSubsurfaceOrder(parent);
    }

    void placeChildAboveParentBottom(RemoteSubsurfaceContext *remote)
    {
        auto *parent = remote->parentExported();
        removeChildFromOrder(remote);
        parent->aboveChildren.prepend(remote);
        syncRemoteSubsurfaceOrder(parent);
    }

    void placeChildBelowParentTop(RemoteSubsurfaceContext *remote)
    {
        auto *parent = remote->parentExported();
        removeChildFromOrder(remote);
        parent->belowChildren.append(remote);
        syncRemoteSubsurfaceOrder(parent);
    }

    void placeChildAboveSibling(RemoteSubsurfaceContext *remote,
                                RemoteSubsurfaceContext *siblingRemote)
    {
        auto *parent = remote->parentExported();
        removeChildFromOrder(remote);

        auto &belowList = parent->belowChildren;
        int idxBelow = belowList.indexOf(siblingRemote);
        if (idxBelow >= 0) {
            belowList.insert(idxBelow + 1, remote);
        } else {
            auto &aboveList = parent->aboveChildren;
            int idxAbove = aboveList.indexOf(siblingRemote);
            aboveList.insert(idxAbove >= 0 ? idxAbove + 1 : aboveList.size(), remote);
        }

        syncRemoteSubsurfaceOrder(parent);
    }

    void placeChildBelowSibling(RemoteSubsurfaceContext *remote,
                                RemoteSubsurfaceContext *siblingRemote)
    {
        auto *parent = remote->parentExported();
        removeChildFromOrder(remote);

        auto &belowList = parent->belowChildren;
        int idxBelow = belowList.indexOf(siblingRemote);
        if (idxBelow >= 0) {
            belowList.insert(idxBelow, remote);
        } else {
            auto &aboveList = parent->aboveChildren;
            int idxAbove = aboveList.indexOf(siblingRemote);
            aboveList.insert(idxAbove > 0 ? idxAbove : 0, remote);
        }

        syncRemoteSubsurfaceOrder(parent);
    }

    // Sibling lookup: find RemoteSubsurfaceContext for a sibling token under same parent
    RemoteSubsurfaceContext *findSibling(RemoteSubsurfaceContext *remote,
                                         const QString &siblingToken) const
    {
        auto *siblingCtx = findExportedByToken(siblingToken);
        if (!siblingCtx)
            return nullptr;
        for (auto *child : childrenOf(remote->parentExported())) {
            if (child->childExported() == siblingCtx)
                return child;
        }
        return nullptr;
    }

    static void safeDestroyRemote(RemoteSubsurfaceContext *remote)
    {
        if (!remote)
            return;
        if (auto *res = remote->resource()) {
            if (res->handle) {
                wl_resource_destroy(res->handle);
                return;
            }
        }
        delete remote;
    }

    // Cleanup
    void cleanupExportedContext(ExportedSurfaceContext *ctx)
    {
        QList<RemoteSubsurfaceContext *> allRemotes = childrenOf(ctx);
        if (ctx->remoteSubsurface)
           allRemotes.append(ctx->remoteSubsurface);

        // Do not emit unmapped signals here: client teardown can already have
        // destroyed one of the WSurface wrappers.
        for (auto *remote : std::as_const(allRemotes)) {
            removeChildFromParent(remote);
            // Prevent RemoteSubsurfaceContext::destroy_resource() from trying
            // to detach the same parent-child relationship a second time when
            // safeDestroyRemote() destroys the role resource below.
            remote->invalidate();
        }

        for (auto *remote : std::as_const(allRemotes))
            safeDestroyRemote(remote);

        unregisterExported(ctx);
    }

    // Track parent state changes so the child's mapping is re-evaluated.
    // The child's own commits are handled by the wlroots role commit callback.
    void trackCommits(RemoteSubsurfaceContext *remote)
    {
        auto *parentSurface = wsurfaceFrom(remote->parentExported());
        if (parentSurface) {
            QObject::connect(parentSurface, &WSurface::commit, remote, [remote] {
                remote->recheckMapping();
            });
            QObject::connect(parentSurface, &WSurface::mappedChanged, remote, [remote] {
                remote->recheckMapping();
            });
        }
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void export_surface(Resource *resource,
                        uint32_t id,
                        struct ::wl_resource *surfaceResource) override
    {
        auto *wlrSurface = wlr_surface_from_resource(surfaceResource);

        if (!wlrSurface) {
            wl_resource_post_error(resource->handle,
                                   error_bad_surface,
                                   "Invalid wl_surface resource");
            return;
        }

        if (isSurfaceExported(wlrSurface)) {
            wl_resource_post_error(resource->handle,
                                   error_bad_surface,
                                   "The wl_surface has already been exported through this manager");
            return;
        }

        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        qCDebug(lcWlRemoteSubsurface) << "Exported surface" << shortToken(token);

        auto *context = new ExportedSurfaceContext(this,
                                                   wlrSurface,
                                                   token,
                                                   resource->client(),
                                                   resource->version(),
                                                   id);

        registerExported(context);
        context->send_surface_token(token);
    }

private:
    QHash<QString, ExportedSurfaceContext *> m_tokens;
};

// ---------------------------------------------------------------------------
// ExportedSurfaceContext implementation
// ---------------------------------------------------------------------------

void ExportedSurfaceContext::handleSurfaceDestroy()
{
    if (!m_manager || m_manager->findExportedByToken(m_token) != this)
        return;

    if (auto *res = resource()) {
        if (res->handle) {
            wl_resource_destroy(res->handle);
            return;
        }
    }
    if (m_manager)
        m_manager->cleanupExportedContext(this);
    delete this;
}

void ExportedSurfaceContext::destroy_resource(Resource *)
{
    if (m_manager)
        m_manager->cleanupExportedContext(this);
    delete this;
}

void ExportedSurfaceContext::create_remote_subsurface(Resource *resource,
                                                      uint32_t id,
                                                      const QString &parent_token)
{
    if (parent_token.isEmpty()) {
        send_parent_rejected(reject_reason_invalid_token, parent_token);
        return;
    }

    // Check if this surface already has a remote subsurface relationship
    if (remoteSubsurface) {
        send_parent_rejected(reject_reason_invalid_token, parent_token);
        return;
    }

    auto *parentContext = m_manager->findExportedByToken(parent_token);
    if (!parentContext) {
        send_parent_rejected(reject_reason_invalid_token, parent_token);
        return;
    }

    if (parentContext == this) {
        send_parent_rejected(reject_reason_cyclic, parent_token);
        return;
    }

    if (m_manager->isDescendantOf(parentContext, this)) {
        send_parent_rejected(reject_reason_cyclic, parent_token);
        return;
    }

    auto *childWlrSurface = m_surface;
    if (!childWlrSurface) {
        // No valid wlr_surface — leave the new remote object inert.
        qCWarning(lcWlRemoteSubsurface)
            << "create_remote_subsurface: child" << shortToken(m_token) << "has no wlr_surface";
        return;
    }

    if (!wlr_surface_set_role(childWlrSurface,
                              &remoteSubsurfaceRole,
                              resource->handle,
                              0 /* bad_surface */)) {
        // Role conflict — wlr_surface_set_role already posted the error.
        // The new treeland_remote_subsurface_v1 object is inert.
        return;
    }

    auto *remote = new RemoteSubsurfaceContext(m_manager,
                                               this,
                                               parentContext,
                                               resource->client(),
                                               resource->version(),
                                               id);

    wlr_surface_set_role_object(childWlrSurface, remote->resource()->handle);

    m_manager->addChildToParent(remote);
    m_manager->trackCommits(remote);
    qCDebug(lcWlRemoteSubsurface) << "Remote subsurface created: child" << shortToken(m_token)
                                  << "→ parent" << shortToken(parent_token);
    remote->recheckMapping();
}

// ---------------------------------------------------------------------------
// RemoteSubsurfaceContext implementation
// ---------------------------------------------------------------------------

RemoteSubsurfaceContext::~RemoteSubsurfaceContext() = default;

// ---------------------------------------------------------------------------
// wlroots role definition (after RemoteSubsurfaceContext is complete)
// ---------------------------------------------------------------------------

static void remote_subsurface_role_commit(struct wlr_surface *surface)
{
    auto *roleRes = QtWaylandServer::treeland_remote_subsurface_v1::Resource::fromResource(
        surface->role_resource);
    if (!roleRes || !roleRes->object())
        return;
    auto *ctx = static_cast<RemoteSubsurfaceContext *>(roleRes->object());
    ctx->recheckMapping();
}

static void remote_subsurface_role_destroy(struct wlr_surface *surface)
{
    // Don't delete the RemoteSubsurfaceContext here.
    // wlroots already called wlr_surface_unmap(); QtWayland will call
    // destroy_resource() which does the full Qt/QHash cleanup.
    Q_UNUSED(surface);
}

void RemoteSubsurfaceContext::ensureSubsurface()
{
    if (m_subsurface)
        return;

    auto *parentSurface = wsurfaceFrom(m_parent);
    auto *childWlr = m_child ? m_child->surface() : nullptr;
    if (!parentSurface || !childWlr)
        return;

    m_subsurface = WRemoteSubsurfaceManagerV1Private::createRemoteSubsurface(parentSurface, childWlr);
    m_subsurface->setPosition(m_position);
    m_manager->syncRemoteSubsurfaceOrder(m_parent);
}

void RemoteSubsurfaceContext::recheckMapping()
{
    if (!m_manager)
        return;

    auto *childQw = m_child ? m_child->surface() : nullptr;

    bool parentMapped = false;
    auto *parentWs = wsurfaceFrom(m_parent);
    parentMapped = parentWs  && parentWs->mapped();

    // ensureSubsurface re-creates this subsurface when the parent WSurface
    // wrapper was destroyed and recreated.  Fast no-op when it still exists.
    ensureSubsurface();

    bool childHasBuffer = childQw && wlr_surface_has_buffer(childQw);
    bool shouldBeMapped = parentMapped && childHasBuffer;

    bool currentlyMapped = m_subsurface && m_subsurface->isMapped();

    if (shouldBeMapped && !currentlyMapped) {
        if (childQw)
            wlr_surface_map(childQw);
        if (m_subsurface)
            m_subsurface->setMapped(true);
        qCDebug(lcWlRemoteSubsurface) << "Mapped: child" << shortToken(m_child->token()) << "parent"
                                      << shortToken(m_parent->token());
        // Recursively recheck children — they may now be eligible for mapping
        for (auto *grandchild : m_manager->childrenOf(m_child)) {
            grandchild->recheckMapping();
        }
    } else if (!shouldBeMapped && currentlyMapped) {
        if (childQw)
            wlr_surface_unmap(childQw);
        cascadeUnmap();
    }
}

void RemoteSubsurfaceContext::cascadeUnmap()
{
    if (!m_subsurface || !m_subsurface->isMapped() || !m_manager)
        return;
    m_subsurface->setMapped(false);
    qCDebug(lcWlRemoteSubsurface) << "Unmapped: child" << shortToken(m_child->token()) << "parent"
                                  << shortToken(m_parent->token());

    // Recursively unmap all remote children of this child
    for (auto *grandchild : m_manager->childrenOf(m_child)) {
        grandchild->cascadeUnmap();
    }
}

void RemoteSubsurfaceContext::set_position(Resource *resource, int32_t x, int32_t y)
{
    Q_UNUSED(resource);
    m_position = QPointF(x, y);
    if (m_subsurface)
        m_subsurface->setPosition(m_position);
}

void RemoteSubsurfaceContext::place_above([[maybe_unused]] Resource *resource,
                                          const QString &sibling_token)
{
    if (sibling_token.isEmpty()) {
        m_manager->placeChildAboveParentTop(this);
        return;
    }

    auto *siblingCtx = m_manager->findExportedByToken(sibling_token);
    if (!siblingCtx) {
        send_invalid_sibling(sibling_token);
        return;
    }

    if (siblingCtx == m_parent) {
        m_manager->placeChildAboveParentBottom(this);
        return;
    }

    auto *siblingRemote = m_manager->findSibling(this, sibling_token);
    if (!siblingRemote) {
        send_invalid_sibling(sibling_token);
        return;
    }

    m_manager->placeChildAboveSibling(this, siblingRemote);
}

void RemoteSubsurfaceContext::place_below([[maybe_unused]] Resource *resource,
                                          const QString &sibling_token)
{
    if (sibling_token.isEmpty()) {
        send_invalid_sibling(sibling_token);
        return;
    }

    auto *siblingCtx = m_manager->findExportedByToken(sibling_token);
    if (!siblingCtx) {
        send_invalid_sibling(sibling_token);
        return;
    }

    if (siblingCtx == m_parent) {
        m_manager->placeChildBelowParentTop(this);
        return;
    }

    auto *siblingRemote = m_manager->findSibling(this, sibling_token);
    if (!siblingRemote) {
        send_invalid_sibling(sibling_token);
        return;
    }

    m_manager->placeChildBelowSibling(this, siblingRemote);
}

void RemoteSubsurfaceContext::destroy_resource(Resource *)
{
    if (m_manager) {
        cascadeUnmap();
        // This is the direct role-object teardown path. Exported-surface
        // cleanup may also reach this object, but it invalidates m_manager
        // before destroying the resource so this detach only runs once.
        m_manager->removeChildFromParent(this);
    }
    delete this;
}

// ---------------------------------------------------------------------------
// WRemoteSubsurfaceManagerV1 — public API
// ---------------------------------------------------------------------------

WRemoteSubsurfaceManagerV1::WRemoteSubsurfaceManagerV1()
    : WObject(*new WRemoteSubsurfaceManagerV1Private(this))
{
}

WRemoteSubsurfaceManagerV1::~WRemoteSubsurfaceManagerV1() = default;

void WRemoteSubsurfaceManagerV1::create(WServer *server)
{
    W_D(WRemoteSubsurfaceManagerV1);
    d->init(server->handle(), InterfaceVersion);
}

void WRemoteSubsurfaceManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WRemoteSubsurfaceManagerV1);
    d->globalRemove();
}

wl_global *WRemoteSubsurfaceManagerV1::global() const
{
    W_DC(WRemoteSubsurfaceManagerV1);
    return d->globalHandle();
}

QByteArrayView WRemoteSubsurfaceManagerV1::interfaceName() const
{
    return QtWaylandServer::treeland_remote_subsurface_manager_v1::interfaceName();
}

WAYLIB_SERVER_END_NAMESPACE

#include "wremotesubsurfacemanagerv1.moc"
