// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wextworkspacemanagerv1.h"
#include "woutput.h"
#include "wscoplistener.h"
#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WExtWorkspaceManagerV1::WExtWorkspaceManagerV1(QObject *parent)
    : QObject(parent)
{
}

WExtWorkspaceManagerV1::~WExtWorkspaceManagerV1()
{
    teardown();
    destroy(nullptr);
}

QByteArrayView WExtWorkspaceManagerV1::interfaceName() const
{
    return "ext_workspace_manager_v1";
}

wlr_ext_workspace_manager_v1 *WExtWorkspaceManagerV1::handle() const
{
    return nativeInterface<wlr_ext_workspace_manager_v1>();
}

void WExtWorkspaceManagerV1::create(WServer *server)
{
    m_handle = wlr_ext_workspace_manager_v1_create(server->handle(), InterfaceVersion);
    if (!m_handle)
        return;
    listeners()->add(&handle()->events.commit, this, &WExtWorkspaceManagerV1::onCommit);
    listeners()->add(&handle()->events.destroy, this, &WExtWorkspaceManagerV1::onNativeDestroyed);
    Q_EMIT initialized();
}

void WExtWorkspaceManagerV1::destroy(WServer *)
{
    if (ownsSocket() && m_handle)
        wlr_ext_workspace_manager_v1_destroy(handle(), false);

    m_handle = nullptr;
}

void WExtWorkspaceManagerV1::onNativeDestroyed()
{
    teardown();
    m_handle = nullptr;
    Q_EMIT invalidated();
}

void WExtWorkspaceManagerV1::onCommit(wlr_ext_workspace_v1_commit_event *event)
{
    Q_EMIT commit(event);
}

wl_global *WExtWorkspaceManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

wlr_ext_workspace_group_handle_v1 *WExtWorkspaceManagerV1::createGroup(uint32_t capabilities)
{
    return handle() ? wlr_ext_workspace_group_handle_v1_create(handle(), capabilities) : nullptr;
}

void WExtWorkspaceManagerV1::removeGroup(wlr_ext_workspace_group_handle_v1 *group)
{
    Q_ASSERT(group && group->manager == handle());
    wlr_ext_workspace_group_handle_v1_destroy(group);
}

void WExtWorkspaceManagerV1::outputEnter(wlr_ext_workspace_group_handle_v1 *group, WOutput *output)
{
    Q_ASSERT(group && group->manager == handle());
    wlr_ext_workspace_group_handle_v1_output_enter(group, output->handle());
}

void WExtWorkspaceManagerV1::outputLeave(wlr_ext_workspace_group_handle_v1 *group, WOutput *output)
{
    Q_ASSERT(group && group->manager == handle());
    wlr_ext_workspace_group_handle_v1_output_leave(group, output->handle());
}

wlr_ext_workspace_handle_v1 *WExtWorkspaceManagerV1::createWorkspace(const QByteArray &id, uint32_t capabilities)
{
    return handle() ? wlr_ext_workspace_handle_v1_create(handle(), id.isEmpty() ? nullptr : id.constData(), capabilities) : nullptr;
}

void WExtWorkspaceManagerV1::removeWorkspace(wlr_ext_workspace_handle_v1 *workspace)
{
    Q_ASSERT(workspace && workspace->manager == handle());
    wlr_ext_workspace_handle_v1_destroy(workspace);
}

void WExtWorkspaceManagerV1::setGroup(wlr_ext_workspace_handle_v1 *workspace, wlr_ext_workspace_group_handle_v1 *group)
{
    Q_ASSERT(workspace && workspace->manager == handle());
    Q_ASSERT(!group || group->manager == handle());
    wlr_ext_workspace_handle_v1_set_group(workspace, group);
}

void WExtWorkspaceManagerV1::setName(wlr_ext_workspace_handle_v1 *workspace, const QString &name)
{
    Q_ASSERT(workspace && workspace->manager == handle());
    const auto utf8 = name.toUtf8();
    wlr_ext_workspace_handle_v1_set_name(workspace, utf8.constData());
}

void WExtWorkspaceManagerV1::setCoordinates(wlr_ext_workspace_handle_v1 *workspace, const QList<uint32_t> &coordinates)
{
    Q_ASSERT(workspace && workspace->manager == handle());
    wlr_ext_workspace_handle_v1_set_coordinates(workspace, coordinates.constData(), coordinates.size());
}

void WExtWorkspaceManagerV1::setState(wlr_ext_workspace_handle_v1 *workspace, bool active, bool urgent, bool hidden)
{
    Q_ASSERT(workspace && workspace->manager == handle());
    wlr_ext_workspace_handle_v1_set_active(workspace, active);
    wlr_ext_workspace_handle_v1_set_urgent(workspace, urgent);
    wlr_ext_workspace_handle_v1_set_hidden(workspace, hidden);
}
WAYLIB_SERVER_END_NAMESPACE
