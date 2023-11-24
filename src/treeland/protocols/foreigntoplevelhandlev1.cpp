// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only


#include "protocols/foreigntoplevelhandlev1.h"
#include "foreign_toplevel_manager_impl.h"

#include <QHash>

#include <qwoutput.h>
#include <qwsignalconnector.h>
#include <qwglobal.h>

QW_USE_NAMESPACE

class TreeLandForeignToplevelHandleV1Private : public QWObjectPrivate
{
public:
    TreeLandForeignToplevelHandleV1Private(treeland_foreign_toplevel_handle_v1 *handle, bool isOwner, TreeLandForeignToplevelHandleV1 *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &TreeLandForeignToplevelHandleV1Private::on_destroy);
        sc.connect(&handle->events.request_maximize, this, &TreeLandForeignToplevelHandleV1Private::on_request_maximize);
        sc.connect(&handle->events.request_minimize, this, &TreeLandForeignToplevelHandleV1Private::on_request_minimize);
        sc.connect(&handle->events.request_activate, this, &TreeLandForeignToplevelHandleV1Private::on_request_activate);
        sc.connect(&handle->events.request_fullscreen, this, &TreeLandForeignToplevelHandleV1Private::on_request_fullscreen);
        sc.connect(&handle->events.request_close, this, &TreeLandForeignToplevelHandleV1Private::on_request_close);
        sc.connect(&handle->events.set_rectangle, this, &TreeLandForeignToplevelHandleV1Private::on_set_rectangle);
    }
    ~TreeLandForeignToplevelHandleV1Private() {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            treeland_foreign_toplevel_handle_v1_destroy(q_func()->handle());
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_request_maximize(void *);
    void on_request_minimize(void *);
    void on_request_activate(void *);
    void on_request_fullscreen(void *);
    void on_request_close(void *);
    void on_set_rectangle(void *);

    static QHash<void*, TreeLandForeignToplevelHandleV1*> map;
    QW_DECLARE_PUBLIC(TreeLandForeignToplevelHandleV1)
    QWSignalConnector sc;
};
QHash<void*, TreeLandForeignToplevelHandleV1*> TreeLandForeignToplevelHandleV1Private::map;

void TreeLandForeignToplevelHandleV1Private::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void TreeLandForeignToplevelHandleV1Private::on_request_maximize(void *data)
{
    Q_EMIT q_func()->requestMaximize(reinterpret_cast<treeland_foreign_toplevel_handle_v1_maximized_event*>(data));
}

void TreeLandForeignToplevelHandleV1Private::on_request_minimize(void *data)
{
    Q_EMIT q_func()->requestMinimize(reinterpret_cast<treeland_foreign_toplevel_handle_v1_minimized_event*>(data));
}

void TreeLandForeignToplevelHandleV1Private::on_request_activate(void *data)
{
    Q_EMIT q_func()->requestActivate(reinterpret_cast<treeland_foreign_toplevel_handle_v1_activated_event*>(data));
}

void TreeLandForeignToplevelHandleV1Private::on_request_fullscreen(void *data)
{
    Q_EMIT q_func()->requestFullscreen(reinterpret_cast<treeland_foreign_toplevel_handle_v1_fullscreen_event*>(data));
}

void TreeLandForeignToplevelHandleV1Private::on_request_close(void *)
{
    Q_EMIT q_func()->requestClose();
}

void TreeLandForeignToplevelHandleV1Private::on_set_rectangle(void *data)
{
    Q_EMIT q_func()->rectangleChanged(reinterpret_cast<treeland_foreign_toplevel_handle_v1_set_rectangle_event*>(data));
}

TreeLandForeignToplevelHandleV1::TreeLandForeignToplevelHandleV1(treeland_foreign_toplevel_handle_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new TreeLandForeignToplevelHandleV1Private(handle, isOwner, this))
{

}

TreeLandForeignToplevelHandleV1 *TreeLandForeignToplevelHandleV1::get(treeland_foreign_toplevel_handle_v1 *handle)
{
    return TreeLandForeignToplevelHandleV1Private::map.value(handle);
}

TreeLandForeignToplevelHandleV1 *TreeLandForeignToplevelHandleV1::from(treeland_foreign_toplevel_handle_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new TreeLandForeignToplevelHandleV1(handle, false);
}

TreeLandForeignToplevelHandleV1 *TreeLandForeignToplevelHandleV1::create(TreeLandForeignToplevelManagerV1 *manager)
{
    auto *handle = treeland_foreign_toplevel_handle_v1_create(manager->handle());
    if (!handle)
        return nullptr;
    return new TreeLandForeignToplevelHandleV1(handle, true);
}

void TreeLandForeignToplevelHandleV1::outputEnter(QWOutput *output)
{
    treeland_foreign_toplevel_handle_v1_output_enter(handle(), output->handle());
}

void TreeLandForeignToplevelHandleV1::outputLeave(QWOutput *output)
{
    treeland_foreign_toplevel_handle_v1_output_leave(handle(), output->handle());
}

void TreeLandForeignToplevelHandleV1::setActivated(bool activated)
{
    treeland_foreign_toplevel_handle_v1_set_activated(handle(), activated);
}

void TreeLandForeignToplevelHandleV1::setAppId(const char *appId)
{
    treeland_foreign_toplevel_handle_v1_set_app_id(handle(), appId);
}

void TreeLandForeignToplevelHandleV1::setFullScreen(bool fullScreen)
{
    treeland_foreign_toplevel_handle_v1_set_fullscreen(handle(), fullScreen);
}

void TreeLandForeignToplevelHandleV1::setMaximized(bool maximized)
{
    treeland_foreign_toplevel_handle_v1_set_maximized(handle(), maximized);
}

void TreeLandForeignToplevelHandleV1::setMinimized(bool minimized)
{
    treeland_foreign_toplevel_handle_v1_set_minimized(handle(), minimized);
}

void TreeLandForeignToplevelHandleV1::setParent(TreeLandForeignToplevelHandleV1 *parent)
{
    treeland_foreign_toplevel_handle_v1_set_parent(handle(), parent->handle());
}

void TreeLandForeignToplevelHandleV1::setTitle(const char *title)
{
    treeland_foreign_toplevel_handle_v1_set_title(handle(), title);
}

void TreeLandForeignToplevelHandleV1::setPid(pid_t pid)
{
    treeland_foreign_toplevel_handle_v1_set_pid(handle(), pid);
}

void TreeLandForeignToplevelHandleV1::setIdentifier(const char *identifier)
{
    treeland_foreign_toplevel_handle_v1_set_identifier(handle(), identifier);
}
