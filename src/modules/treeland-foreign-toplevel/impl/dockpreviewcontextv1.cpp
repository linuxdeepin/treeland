// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreign_toplevel_manager_impl.h"
#include "foreigntoplevelhandlev1.h"

#include <qwglobal.h>
#include <qwoutput.h>
#include <qwsignalconnector.h>

#include <QHash>

QW_USE_NAMESPACE

class TreeLandDockPreviewContextV1Private : public QWObjectPrivate
{
public:
    TreeLandDockPreviewContextV1Private(treeland_dock_preview_context_v1 *handle,
                                        bool isOwner,
                                        TreeLandDockPreviewContextV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &TreeLandDockPreviewContextV1Private::on_destroy);
        sc.connect(&handle->events.request_show,
                   this,
                   &TreeLandDockPreviewContextV1Private::on_request_show);
        sc.connect(&handle->events.request_close,
                   this,
                   &TreeLandDockPreviewContextV1Private::on_request_close);
    }

    ~TreeLandDockPreviewContextV1Private()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            treeland_dock_preview_context_v1_destroy(q_func()->handle());
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_request_show(void *);
    void on_request_close(void *);

    static QHash<void *, TreeLandDockPreviewContextV1 *> map;
    QW_DECLARE_PUBLIC(TreeLandDockPreviewContextV1)
    QWSignalConnector sc;
};

QHash<void *, TreeLandDockPreviewContextV1 *> TreeLandDockPreviewContextV1Private::map;

void TreeLandDockPreviewContextV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void TreeLandDockPreviewContextV1Private::on_request_show(void *data)
{
    struct treeland_dock_preview_context_v1_preview_event *event =
        static_cast<treeland_dock_preview_context_v1_preview_event *>(data);
    Q_EMIT q_func()->requestShow(event);
}

void TreeLandDockPreviewContextV1Private::on_request_close([[maybe_unused]] void *data)
{
    Q_EMIT q_func()->requestClose();
}

TreeLandDockPreviewContextV1::TreeLandDockPreviewContextV1(treeland_dock_preview_context_v1 *handle,
                                                           bool isOwner)
    : QObject(nullptr)
    , QWObject(*new TreeLandDockPreviewContextV1Private(handle, isOwner, this))
{
}

TreeLandDockPreviewContextV1 *TreeLandDockPreviewContextV1::get(
    treeland_dock_preview_context_v1 *handle)
{
    return TreeLandDockPreviewContextV1Private::map.value(handle);
}

TreeLandDockPreviewContextV1 *TreeLandDockPreviewContextV1::from(
    treeland_dock_preview_context_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new TreeLandDockPreviewContextV1(handle, false);
}

void TreeLandDockPreviewContextV1::enter()
{
    treeland_dock_preview_context_v1_enter(handle());
}

void TreeLandDockPreviewContextV1::leave()
{
    treeland_dock_preview_context_v1_leave(handle());
}
