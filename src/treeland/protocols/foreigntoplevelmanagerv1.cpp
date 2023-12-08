// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelhandlev1.h"

#include "foreign_toplevel_manager_impl.h"

#include <qwsignalconnector.h>
#include <qwglobal.h>
#include <qwdisplay.h>

#include <QHash>

QW_USE_NAMESPACE

class TreeLandForeignToplevelManagerV1Private : public QWObjectPrivate
{
public:
    TreeLandForeignToplevelManagerV1Private(treeland_foreign_toplevel_manager_v1 *handle, bool isOwner, TreeLandForeignToplevelManagerV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &TreeLandForeignToplevelManagerV1Private::on_destroy);
        sc.connect(&handle->events.dock_preview_created, this, &TreeLandForeignToplevelManagerV1Private::on_dock_preview_created);
    }
    ~TreeLandForeignToplevelManagerV1Private() {
        if (!m_handle)
            return;
        destroy();
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_dock_preview_created(void *);

    static QHash<void*, TreeLandForeignToplevelManagerV1*> map;
    QW_DECLARE_PUBLIC(TreeLandForeignToplevelManagerV1)
    QWSignalConnector sc;
};
QHash<void*, TreeLandForeignToplevelManagerV1*> TreeLandForeignToplevelManagerV1Private::map;

void TreeLandForeignToplevelManagerV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void TreeLandForeignToplevelManagerV1Private::on_dock_preview_created(void *data)
{
    auto *context = TreeLandDockPreviewContextV1::from(static_cast<treeland_dock_preview_context_v1*>(data));
    Q_EMIT q_func()->dockPreviewContextCreated(context);
}

TreeLandForeignToplevelManagerV1::TreeLandForeignToplevelManagerV1(treeland_foreign_toplevel_manager_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new TreeLandForeignToplevelManagerV1Private(handle, isOwner, this))
{

}

TreeLandForeignToplevelManagerV1 *TreeLandForeignToplevelManagerV1::get(treeland_foreign_toplevel_manager_v1 *handle)
{
    return TreeLandForeignToplevelManagerV1Private::map.value(handle);
}

TreeLandForeignToplevelManagerV1 *TreeLandForeignToplevelManagerV1::from(treeland_foreign_toplevel_manager_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new TreeLandForeignToplevelManagerV1(handle, false);
}

TreeLandForeignToplevelManagerV1 *TreeLandForeignToplevelManagerV1::create(QWDisplay *display)
{
    auto *handle = treeland_foreign_toplevel_manager_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new TreeLandForeignToplevelManagerV1(handle, true);
}
