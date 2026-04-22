// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winewindowmanagement.h"

#include "qwayland-server-treeland-wine-window-management-v1.h"

#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wxdgtoplevelsurface.h>

#include <qwdisplay.h>
#include <qwxdgshell.h>

#include <QHash>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(treelandWineWindowManagement,
                   "treeland.wine.window.management",
                   QtInfoMsg)

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

    WineWindowControl *controlForId(uint32_t id) const { return m_idToControl.value(id, nullptr); }

    void registerControl(uint32_t id, WXdgToplevelSurface *toplevel, WineWindowControl *control)
    {
        m_idToControl.insert(id, control);
        m_toplevelToControl.insert(toplevel, control);
        m_controlToToplevel.insert(control, toplevel);
    }

    void removeControl(WineWindowControl *control)
    {
        auto it = m_controlToToplevel.find(control);
        if (it == m_controlToToplevel.end())
            return;
        WXdgToplevelSurface *toplevel = it.value();
        m_controlToToplevel.erase(it);
        m_toplevelToControl.remove(toplevel);

        // remove from id map
        for (auto idIt = m_idToControl.begin(); idIt != m_idToControl.end(); ++idIt) {
            if (idIt.value() == control) {
                m_idToControl.erase(idIt);
                break;
            }
        }
    }

protected:
    void destroy_global() override
    {
        qCDebug(treelandWineWindowManagement) << "WineWindowManagementManager global destroyed";
    }

    void destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }

    void get_window_control(Resource *resource,
                            uint32_t id,
                            struct ::wl_resource *toplevelResource) override;

private:
    WineWindowManagementManager *q = nullptr;
    uint32_t m_nextId = 0;
    QHash<uint32_t, WineWindowControl *> m_idToControl;
    QHash<WXdgToplevelSurface *, WineWindowControl *> m_toplevelToControl;
    QHash<WineWindowControl *, WXdgToplevelSurface *> m_controlToToplevel;
};

class WineWindowControl
    : public QObject
    , public QtWaylandServer::treeland_wine_window_control_v1
{
public:
    WineWindowControl(QObject *parent,
                      WineWindowManagementManagerPrivate *manager,
                      uint32_t windowId,
                      WXdgToplevelSurface *toplevel,
                      SurfaceWrapper *wrapper,
                      wl_client *client,
                      int version,
                      uint32_t id)
        : QObject(parent)
        , QtWaylandServer::treeland_wine_window_control_v1(client, id, version)
        , m_manager(manager)
        , m_windowId(windowId)
        , m_toplevel(toplevel)
        , m_wrapper(wrapper)
    {
        if (m_toplevel) {
            connect(m_toplevel, &QObject::destroyed, this, [this] {
                m_alive = false;
                m_toplevel = nullptr;
                m_wrapper = nullptr;
            });
        }

        if (m_wrapper) {
            connect(m_wrapper, &QObject::destroyed, this, [this] {
                m_wrapper = nullptr;
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

        qCDebug(treelandWineWindowManagement)
            << "set_position" << x << y << "for window_id" << m_windowId;

        m_wrapper->setPositionAutomatic(false);
        m_wrapper->setPosition(QPointF(x, y));

        sendConfigurePosition();
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
            sendConfigureStacking();
            break;
        case z_order_op_hwnd_topmost:
            m_wrapper->setAlwaysOnTop(true);
            applyHwndTop();
            sendConfigureStacking();
            break;
        case z_order_op_hwnd_notopmost:
            m_wrapper->setAlwaysOnTop(false);
            applyHwndTop();
            sendConfigureStacking();
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
    WXdgToplevelSurface *m_toplevel = nullptr;
    SurfaceWrapper *m_wrapper = nullptr;
    bool m_alive = true;
};

void WineWindowManagementManagerPrivate::get_window_control(Resource *resource,
                                                            uint32_t id,
                                                            struct ::wl_resource *toplevelResource)
{
    auto *qXdgToplevel = qw_xdg_toplevel::from_resource(toplevelResource);
    auto *xdgToplevel = WXdgToplevelSurface::fromHandle(qXdgToplevel);
    auto *helper = Helper::instance();
    auto *root = helper ? helper->rootSurfaceContainer() : nullptr;
    auto *wrapper = (root && xdgToplevel) ? root->getSurface(xdgToplevel) : nullptr;

    if (!qXdgToplevel || !xdgToplevel || !wrapper) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_MANAGER_V1_ERROR_DEFUNCT_TOPLEVEL,
                               "invalid or defunct toplevel");
        return;
    }

    if (m_toplevelToControl.contains(xdgToplevel)) {
        wl_resource_post_error(resource->handle,
                               TREELAND_WINE_WINDOW_MANAGER_V1_ERROR_TOPLEVEL_ALREADY_CONTROLLED,
                               "toplevel already controlled");
        return;
    }

    uint32_t windowId = nextWindowId();
    auto *control = new WineWindowControl(q,
                                          this,
                                          windowId,
                                          xdgToplevel,
                                          wrapper,
                                          resource->client(),
                                          resource->version(),
                                          id);
    registerControl(windowId, xdgToplevel, control);

    qCDebug(treelandWineWindowManagement)
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
    qCDebug(treelandWineWindowManagement) << "WineWindowManagementManager created";
}

void WineWindowManagementManager::destroy(WServer * /*server*/)
{
    d->globalRemove();
    qCDebug(treelandWineWindowManagement) << "WineWindowManagementManager destroyed";
}

wl_global *WineWindowManagementManager::global() const
{
    return d->globalHandle();
}

QByteArrayView WineWindowManagementManager::interfaceName() const
{
    return QtWaylandServer::treeland_wine_window_manager_v1::interface()->name;
}
