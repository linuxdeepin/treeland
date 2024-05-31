// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowmanagement.h"
#include "impl/window_management_impl.h"

#include <wserver.h>

#include <qwdisplay.h>

#include <QDebug>
#include <QQmlInfo>

extern "C" {
#include <wayland-server-core.h>
}

TreelandWindowManagement::TreelandWindowManagement(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{
    qRegisterMetaType<DesktopState>("DesktopState");
}

TreelandWindowManagement::DesktopState TreelandWindowManagement::desktopState()
{
    // TODO: When the protocol is not initialized,
    // qml calls the current interface m_handle is empty
    return m_handle ? static_cast<DesktopState>(m_handle->state) : DesktopState::Normal;
}

void TreelandWindowManagement::setDesktopState(DesktopState state)
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

    m_handle->set_desktop(s);
    Q_EMIT desktopStateChanged();

    qmlWarning(this) << QString("Try to show desktop state (%1)!").arg(s);
}

WServerInterface *TreelandWindowManagement::create()
{
    m_handle = treeland_window_management_v1::create(server()->handle());

    connect(m_handle,
            &treeland_window_management_v1::requestShowDesktop,
            this,
            &TreelandWindowManagement::requestShowDesktop);
    return new WServerInterface(m_handle, m_handle->global);
}
