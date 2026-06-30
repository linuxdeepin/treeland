// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winewindowmanagement.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "qwayland-server-treeland-wine-window-management-v1.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wxdgtoplevelsurface.h>

#include <qwdisplay.h>
#include <qwxdgshell.h>

#include <QList>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class WineWindowControl;

class WineWindowManagerPrivate : public QtWaylandServer::treeland_wine_window_manager_v1
{
public:
    explicit WineWindowManagerPrivate(WineWindowManager *q)
        : q(q)
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    uint32_t nextWindowId()
    {
        return ++m_nextId;
    }

    WineWindowControl *controlForId(uint32_t id) const
    {
        for (const auto &entry : m_controls)
            if (entry.id == id)
                return entry.control;
        return nullptr;
    }

    void registerControl(uint32_t id, WXdgToplevelSurface *toplevel, WineWindowControl *control)
    {
        m_controls.append({ id, toplevel, control });
    }

    void removeControl(WineWindowControl *control)
    {
        for (auto it = m_controls.begin(); it != m_controls.end(); ++it) {
            if (it->control == control) {
                m_controls.erase(it);
                return;
            }
        }
    }

protected:
    void destroy_global() override
    {
        qCDebug(lcTlProtocol) << "WineWindowManager global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_window_control(Resource *resource,
                            uint32_t id,
                            struct ::wl_resource *toplevelResource) override;

private:
    struct ControlEntry
    {
        uint32_t id;
        WXdgToplevelSurface *toplevel{ nullptr };
        WineWindowControl *control{ nullptr };
    };

    WineWindowManager *q{ nullptr };
    uint32_t m_nextId = 0;
    QList<ControlEntry> m_controls;
};

class WineWindowControl
    : public QObject
    , public QtWaylandServer::treeland_wine_window_control_v1
{
public:
    WineWindowControl(QObject *parent,
                      WineWindowManagerPrivate *manager,
                      uint32_t windowId,
                      SurfaceWrapper *wrapper,
                      wl_client *client,
                      int version,
                      uint32_t id)
        : QObject(parent)
        , QtWaylandServer::treeland_wine_window_control_v1(client, id, version)
        , m_manager(manager)
        , m_windowId(windowId)
        , m_wrapper(wrapper)
    {
        Q_ASSERT_X(m_wrapper, Q_FUNC_INFO, "wrapper must not be null");
        Q_ASSERT_X(m_wrapper->shellSurface(),
                   Q_FUNC_INFO,
                   "wrapper->shellSurface() must not be null");

        connect(m_wrapper, &SurfaceWrapper::aboutToBeInvalidated, this, [this] {
            if (m_manager) {
                m_manager->removeControl(this);
                m_manager = nullptr;
            }
            m_wrapper = nullptr;
        });
        connect(m_wrapper, &QQuickItem::xChanged, this, [this] {
            if (!m_suppressPositionEvents)
                sendConfigurePosition();
        });
        connect(m_wrapper, &QQuickItem::yChanged, this, [this] {
            if (!m_suppressPositionEvents)
                sendConfigurePosition();
        });
        connect(m_wrapper, &SurfaceWrapper::alwaysOnTopChanged, this, [this] {
            sendConfigureStacking();
        });

        // Initial event sequence: window_id → configure_position → configure_stacking
        send_window_id(m_windowId);
        sendConfigurePosition();
        sendConfigureStacking();
    }

    uint32_t windowId() const
    {
        return m_windowId;
    }

    SurfaceWrapper *wrapper() const
    {
        return m_wrapper;
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource([[maybe_unused]] Resource *resource) override
    {
        if (m_manager) {
            m_manager->removeControl(this);
            m_manager = nullptr;
        }
        delete this;
    }

    void set_position([[maybe_unused]] Resource *resource,
                      int32_t x,
                      int32_t y,
                      uint32_t serial) override
    {
        if (!m_wrapper) {
            return;
        }

        qCDebug(lcTlProtocol) << "set_position serial" << x << y << serial << "for window_id"
                              << m_windowId;

        m_wrapper->setPositionAutomatic(false);
        // Suppress only this class's xChanged/yChanged handlers to avoid
        // emitting a serial=0 event alongside the client-requested one.
        m_suppressPositionEvents = true;
        m_wrapper->setPosition(QPointF(x, y));
        m_suppressPositionEvents = false;
        // Echo the client serial back in configure_position
        sendConfigurePosition(serial);
    }

    void set_z_order([[maybe_unused]] Resource *resource, uint32_t op, uint32_t sibling_id) override
    {
        if (!m_wrapper) {
            return;
        }

        const auto zOrderOp =
            static_cast<QtWaylandServer::treeland_wine_window_control_v1::z_order_op>(op);

        // Validate sibling_id: must be 0 for non-insert_after ops
        if (op != z_order_op_hwnd_insert_after && sibling_id != 0) {
            wl_resource_post_error(resource->handle,
                                   error_invalid_sibling,
                                   "sibling_id must be 0 for op %u",
                                   op);
            return;
        }

        switch (zOrderOp) {
        case z_order_op_hwnd_top:
            applyHwndTop();
            // hwnd_top does not change tier → no configure_stacking
            break;
        case z_order_op_hwnd_bottom:
            applyHwndBottom();
            break;
        case z_order_op_hwnd_topmost:
            m_wrapper->setAlwaysOnTop(true);
            applyHwndTop();
            break;
        case z_order_op_hwnd_notopmost:
            m_wrapper->setAlwaysOnTop(false);
            applyHwndTop();
            break;
        case z_order_op_hwnd_insert_after:
            applyHwndInsertAfter(sibling_id);
            // hwnd_insert_after does not change tier → no configure_stacking
            break;
        }
    }

private:
    // serial=0 means compositor-initiated (not in response to a client set_position)
    void sendConfigurePosition(uint32_t serial = 0)
    {
        if (m_wrapper) {
            send_configure_position(static_cast<int32_t>(m_wrapper->x()),
                                    static_cast<int32_t>(m_wrapper->y()),
                                    serial);
        } else {
            send_configure_position(0, 0, serial);
        }
    }

    void sendConfigureStacking()
    {
        bool topmost = m_wrapper && m_wrapper->alwaysOnTop();
        send_configure_stacking(topmost ? 1u : 0u);
    }

    // Raise to top of current stacking tier (no tier change)
    void applyHwndTop()
    {
        if (!m_wrapper)
            return;
        m_wrapper->stackToLast();
    }

    // Lower to absolute bottom, clear topmost tier.
    void applyHwndBottom()
    {
        if (!m_wrapper)
            return;
        m_wrapper->setAlwaysOnTop(false);
        m_wrapper->stackToFirst();
    }

    // Place this window directly below the identified sibling.
    // Matches Windows SetWindowPos semantics: top-level windows may only be
    // ordered relative to other top-level peers, and child windows only
    // relative to same-parent siblings. Cross-group requests are ignored.
    void applyHwndInsertAfter(uint32_t siblingId)
    {
        if (!m_manager || !m_wrapper)
            return;

        WineWindowControl *sibling = m_manager->controlForId(siblingId);
        // If sibling is not found, the request has no effect (e.g. the
        // sibling window was destroyed before this message arrived).
        if (!sibling || !sibling->wrapper())
            return;
        if (sibling == this)
            return;

        SurfaceWrapper *siblingWrapper = sibling->wrapper();

        // Protocol set_z_order: child windows may only be ordered relative to
        // same-parent siblings. An independent top-level has no parentSurface,
        // so the check only fires when m_wrapper is a child and its top-level
        // ancestor differs from the sibling's.
        if (m_wrapper->parentSurface()
            && topLevelSurface(m_wrapper) != topLevelSurface(siblingWrapper)) {
            qCWarning(lcTlProtocol)
                << "hwnd_insert_after: child window" << m_windowId << "and sibling_id" << siblingId
                << "belong to different top-level groups; ignoring";
            return;
        }

        // Protocol set_z_order: hwnd_insert_after must not mix tiers.
        if (m_wrapper->effectiveAlwaysOnTop() != siblingWrapper->effectiveAlwaysOnTop()) {
            qCWarning(lcTlProtocol) << "hwnd_insert_after: window" << m_windowId << "and sibling_id"
                                    << siblingId << "belong to different tiers; ignoring";
            return;
        }

        // Safety check: both windows must share the same Qt parent container.
        if (!m_wrapper->parentItem() || siblingWrapper->parentItem() != m_wrapper->parentItem()) {
            qCWarning(lcTlProtocol) << "hwnd_insert_after: window" << m_windowId << "and sibling_id"
                                    << siblingId << "do not share a parent container; ignoring";
            return;
        }

        // Place this window before the sibling (visually below sibling).
        if (!m_wrapper->stackBefore(siblingWrapper)) {
            qCWarning(lcTlProtocol) << "hwnd_insert_after: stackBefore failed for window"
                                    << m_windowId << "(sibling_id =" << siblingId << ")";
        }
    }

    static SurfaceWrapper *topLevelSurface(SurfaceWrapper *wrapper)
    {
        while (wrapper && wrapper->parentSurface())
            wrapper = wrapper->parentSurface();
        return wrapper;
    }

private:
    WineWindowManagerPrivate *m_manager = nullptr;
    uint32_t m_windowId = 0;
    SurfaceWrapper *m_wrapper = nullptr;
    bool m_suppressPositionEvents = false;
};

void WineWindowManagerPrivate::get_window_control(Resource *resource,
                                                  uint32_t id,
                                                  struct ::wl_resource *toplevelResource)
{
    auto *qXdgToplevel = qw_xdg_toplevel::from_resource(toplevelResource);
    if (!qXdgToplevel) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_MANAGER_V1_ERROR_DEFUNCT_TOPLEVEL,
                               "invalid or defunct toplevel");
        return;
    }

    auto *xdgToplevel = WXdgToplevelSurface::fromHandle(qXdgToplevel);
    auto *helper = Helper::instance();
    auto *root = helper ? helper->rootSurfaceContainer() : nullptr;
    auto *wrapper = (root && xdgToplevel) ? root->getSurface(xdgToplevel) : nullptr;

    if (!xdgToplevel || !wrapper || !wrapper->shellSurface()) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_MANAGER_V1_ERROR_DEFUNCT_TOPLEVEL,
                               "invalid or defunct toplevel");
        return;
    }

    const bool alreadyControlled = [&] {
        for (const auto &e : m_controls)
            if (e.toplevel == xdgToplevel)
                return true;
        return false;
    }();
    if (alreadyControlled) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_MANAGER_V1_ERROR_TOPLEVEL_ALREADY_CONTROLLED,
                               "toplevel already controlled");
        return;
    }

    uint32_t windowId = nextWindowId();
    auto *control = new WineWindowControl(q,
                                          this,
                                          windowId,
                                          wrapper,
                                          resource->client(),
                                          resource->version(),
                                          id);
    registerControl(windowId, xdgToplevel, control);

    qCDebug(lcTlProtocol) << "Created WineWindowControl for toplevel, window_id =" << windowId;
}

// ---------------------------------------------------------------------------
// WineWindowManager public interface
// ---------------------------------------------------------------------------

WineWindowManager::WineWindowManager(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<WineWindowManagerPrivate>(this))
{
}

WineWindowManager::~WineWindowManager() = default;

void WineWindowManager::create(WServer *server)
{
    d->init(*server->handle(), InterfaceVersion);
}

void WineWindowManager::destroy(WServer * /*server*/)
{
    d->globalRemove();
}

wl_global *WineWindowManager::global() const
{
    return d->globalHandle();
}

QByteArrayView WineWindowManager::interfaceName() const
{
    return QtWaylandServer::treeland_wine_window_manager_v1::interfaceName();
}
