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
#define TREELAND_WINE_WINDOW_MANAGEMENT_V1_VERSION 1

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class WineWindowControl;

class WineWindowManagementManagerPrivate
    : public QtWaylandServer::treeland_wine_window_manager_v1
{
public:
    explicit WineWindowManagementManagerPrivate(WineWindowManagementManager *q)
        : q(q)
    {
    }

    wl_global *globalHandle() const { return m_global; }

    uint32_t nextWindowId() { return ++m_nextId; }

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
        qCDebug(treelandProtocol) << "WineWindowManagementManager global destroyed";
    }

    void destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }

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

    WineWindowManagementManager *q{ nullptr };
    uint32_t m_nextId = 0;
    QList<ControlEntry> m_controls;
};

class WineWindowControl
    : public QObject
    , public QtWaylandServer::treeland_wine_window_control_v1
{
public:
    WineWindowControl(QObject *parent,
                      WineWindowManagementManagerPrivate *manager,
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
        if (m_wrapper) {
            connect(m_wrapper, &SurfaceWrapper::aboutToBeInvalidated, this, [this] {
                m_alive = false;
                if (m_manager) {
                    m_manager->removeControl(this);
                    m_manager = nullptr;
                }
                m_wrapper = nullptr;
            });
            connect(m_wrapper, &QObject::destroyed, this, [this] {
                m_wrapper = nullptr;
            });
            connect(m_wrapper, &QQuickItem::xChanged, this, [this] {
                sendConfigurePosition();
            });
            connect(m_wrapper, &QQuickItem::yChanged, this, [this] {
                sendConfigurePosition();
            });
            connect(m_wrapper, &SurfaceWrapper::alwaysOnTopChanged, this, [this] {
                sendConfigureStacking();
            });
        }

        // Initial event sequence: window_id → configure_position → configure_stacking
        send_window_id(m_windowId);
        sendConfigurePosition();
        sendConfigureStacking();
    }

    uint32_t windowId() const { return m_windowId; }

    SurfaceWrapper *wrapper() const { return m_wrapper; }

protected:
    void destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }

    void destroy_resource([[maybe_unused]] Resource *resource) override
    {
        if (m_manager) {
            m_manager->removeControl(this);
            m_manager = nullptr;
        }
        delete this;
    }

    void set_position([[maybe_unused]] Resource *resource, int32_t x, int32_t y) override
    {
        if (!m_alive || !m_wrapper) {
            return;
        }

        qCDebug(treelandProtocol)
            << "set_position" << x << y << "for window_id" << m_windowId;

        m_wrapper->setPositionAutomatic(false);
        m_wrapper->setPosition(QPointF(x, y));
    }

    void set_z_order([[maybe_unused]] Resource *resource,
                     uint32_t op,
                     uint32_t sibling_id) override
    {
        if (!m_alive || !m_wrapper) {
            return;
        }

        const auto zOrderOp =
            static_cast<QtWaylandServer::treeland_wine_window_control_v1::z_order_op>(op);

        // Validate sibling_id: must be 0 for non-insert_after ops
        if (op != z_order_op_hwnd_insert_after && sibling_id != 0) {
            wl_resource_post_error(resource->handle,
                                   error_invalid_sibling,
                                   "sibling_id must be 0 for op %u", op);
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
    void sendConfigurePosition()
    {
        if (m_wrapper) {
            send_configure_position(static_cast<int32_t>(m_wrapper->x()),
                                    static_cast<int32_t>(m_wrapper->y()));
        } else {
            send_configure_position(0, 0);
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
        if (!m_wrapper || !m_wrapper->parentItem())
            return;
        auto *parent = m_wrapper->parentItem();
        const auto children = parent->childItems();
        if (children.isEmpty() || children.last() == m_wrapper)
            return;
        // stackAfter(last) brings this above the last child
        m_wrapper->QQuickItem::stackAfter(children.last());
    }

    // Lower to absolute bottom, clear topmost tier
    void applyHwndBottom()
    {
        if (!m_wrapper)
            return;
        m_wrapper->setAlwaysOnTop(false);
        if (!m_wrapper->parentItem())
            return;
        auto *parent = m_wrapper->parentItem();
        const auto children = parent->childItems();
        if (children.isEmpty() || children.first() == m_wrapper)
            return;
        m_wrapper->QQuickItem::stackBefore(children.first());
    }

    // Place this window directly below the identified sibling
    void applyHwndInsertAfter(uint32_t siblingId)
    {
        if (!m_manager || !m_wrapper)
            return;

        WineWindowControl *sibling = m_manager->controlForId(siblingId);
        // If sibling not found or in different tier, fall back to hwnd_top
        if (!sibling || !sibling->wrapper()) {
            applyHwndTop();
            return;
        }

        SurfaceWrapper *siblingWrapper = sibling->wrapper();
        // Both must share the same parent container
        if (!m_wrapper->parentItem() || siblingWrapper->parentItem() != m_wrapper->parentItem()) {
            applyHwndTop();
            return;
        }

        // Place this window before the sibling (visually below sibling)
        m_wrapper->stackBefore(siblingWrapper);
    }

private:
    WineWindowManagementManagerPrivate *m_manager = nullptr;
    uint32_t m_windowId = 0;
    SurfaceWrapper *m_wrapper = nullptr;
    bool m_alive = true;
};

void WineWindowManagementManagerPrivate::get_window_control(Resource *resource,
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

    if (!xdgToplevel || !wrapper) {
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

    qCDebug(treelandProtocol)
        << "Created WineWindowControl for toplevel, window_id =" << windowId;
}

// ---------------------------------------------------------------------------
// WineWindowManagementManager public interface
// ---------------------------------------------------------------------------

WineWindowManagementManager::WineWindowManagementManager(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<WineWindowManagementManagerPrivate>(this))
{
}

WineWindowManagementManager::~WineWindowManagementManager() = default;

void WineWindowManagementManager::create(WServer *server)
{
    d->init(*server->handle(), TREELAND_WINE_WINDOW_MANAGEMENT_V1_VERSION);
}

void WineWindowManagementManager::destroy(WServer * /*server*/)
{
    d->globalRemove();
}

wl_global *WineWindowManagementManager::global() const
{
    return d->globalHandle();
}

QByteArrayView WineWindowManagementManager::interfaceName() const
{
    return QtWaylandServer::treeland_wine_window_manager_v1::interface()->name;
}
