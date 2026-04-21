// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowmanagementinterfacev1.h"
#include "qwayland-server-treeland-window-management-v1.h"

#include <wserver.h>

#include <qwdisplay.h>

#include <QDebug>
#include <QQmlInfo>

#include <wayland-server-core.h>

class WindowManagementInterfaceV1Private : public QtWaylandServer::treeland_window_management_v1
{
public:
    WindowManagementInterfaceV1Private(WindowManagementInterfaceV1 *_q);
    wl_global *global() const;

    WindowManagementInterfaceV1 *q;
    QList<Resource *> m_resource;

    uint32_t state = 0; // desktop_state 0: normal, 1: show, 2: preview;
protected:
    void bind_resource(Resource *resource) override;
    void destroy_resource(Resource *resource) override;
    void destroy_global() override;
    void destroy(Resource *resource) override;
    void set_desktop(Resource *resource, uint32_t state) override;
};

WindowManagementInterfaceV1Private::WindowManagementInterfaceV1Private(WindowManagementInterfaceV1 *_q)
    : q(_q)
{
}

wl_global *WindowManagementInterfaceV1Private::global() const
{
    return m_global;
}

void WindowManagementInterfaceV1Private::bind_resource(Resource *resource)
{
    m_resource.append(resource);
    send_show_desktop(resource->handle, state);
}

void WindowManagementInterfaceV1Private::destroy_resource(Resource *resource)
{
    m_resource.removeOne(resource);
}

void WindowManagementInterfaceV1Private::destroy_global() {
    m_resource.clear();
}

void WindowManagementInterfaceV1Private::destroy(Resource *resource) {
    wl_resource_destroy(resource->handle);
}

void WindowManagementInterfaceV1Private::set_desktop([[maybe_unused]] Resource *resource, uint32_t state)
{
    Q_EMIT q->requestShowDesktop(state);
}

WindowManagementInterfaceV1::WindowManagementInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new WindowManagementInterfaceV1Private(this))
{
    qRegisterMetaType<DesktopState>("DesktopState");
}

WindowManagementInterfaceV1::~WindowManagementInterfaceV1() = default;

WindowManagementInterfaceV1::DesktopState WindowManagementInterfaceV1::desktopState()
{
    // TODO: When the protocol is not initialized,
    // qml calls the current interface m_handle is empty
    return d->global() ? static_cast<DesktopState>(d->state) : DesktopState::Normal;
}

void WindowManagementInterfaceV1::setDesktopState(DesktopState state)
{
    uint32_t s = 0;
    switch (state) {
    case DesktopState::Normal:
        s = TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_NORMAL;
        break;
    case DesktopState::Show:
        s = TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_SHOW;
        break;
    case DesktopState::Preview:
        s = TREELAND_WINDOW_MANAGEMENT_V1_DESKTOP_STATE_PREVIEW_SHOW;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    d->state = s;
    for (auto resource : d->m_resource) {
        d->send_show_desktop(resource->handle, s);
    }

    Q_EMIT desktopStateChanged();

    qmlWarning(this) << QString("Try to show desktop state (%1)!").arg(s);
}

void WindowManagementInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void WindowManagementInterfaceV1::destroy([[maybe_unused]] WServer *server) {
    d->globalRemove();
}

wl_global *WindowManagementInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView WindowManagementInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}
