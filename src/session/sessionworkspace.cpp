// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionworkspace.h"
#include "session.h"
#include "core/rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "workspace/workspace.h"
#include "output/output.h"
#include "surface/surfacewrapper.h"
#include "treelanduserconfig.hpp"

#include <woutput.h>
#include <wlr_all.h>
#include <QUuid>
#include <algorithm>
#include <functional>

SessionWorkspace::SessionWorkspace(Session *session, WaylandServer *runtime)
    : SessionComponent(session)
    , m_root(runtime->rootSurfaceContainer())
    , m_workspace(new Workspace(runtime->rootSurfaceContainer(), session->config()))
    , m_protocolIdPrefix(QUuid::createUuid().toByteArray(QUuid::WithoutBraces))
{
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_workspace->setSize(runtime->rootSurfaceContainer()->size());
    deactivate();
    initializeActiveState();
}

SessionWorkspace::~SessionWorkspace()
{
    if (!m_workspace)
        return;

    deactivate();
    auto *runtime = WaylandServer::instance();
    // The runtime may still reference the last active workspace. In that case
    // its visual parent retains ownership, as it does during runtime shutdown.
    if (!runtime || runtime->workspace() == m_workspace)
        return;

    auto *workspace = m_workspace.data();
    if (workspace->surfaces().isEmpty()) {
        workspace->deleteLater();
    } else {
        connect(workspace, &SurfaceContainer::surfaceRemoved, workspace, [workspace] {
            if (workspace->surfaces().isEmpty())
                workspace->deleteLater();
        });
    }
}

Workspace *SessionWorkspace::workspace() const
{
    return m_workspace;
}

void SessionWorkspace::activate()
{
    m_workspace->setEnabled(true);
    m_workspace->setVisible(true);
}

void SessionWorkspace::deactivate()
{
    m_workspace->setEnabled(false);
    m_workspace->resetView();
    m_workspace->setVisible(false);
}

void SessionWorkspace::installProtocol()
{
    Q_ASSERT(!m_protocol);
    m_protocol = new WExtWorkspaceManagerV1;
    // The socket ancestry also permits parent-domain globals; require the exact
    // Session here so child sessions cannot enumerate their parent's workspaces.
    m_protocol->setFilter(std::bind_front(&Session::ownsClient, session()));
    connect(m_protocol, &WExtWorkspaceManagerV1::initialized,
            this, &SessionWorkspace::initializeProtocol);
    connect(m_protocol, &WExtWorkspaceManagerV1::invalidated,
            this, &SessionWorkspace::invalidateProtocol);
    connect(m_protocol, &WExtWorkspaceManagerV1::commit,
            this, &SessionWorkspace::handleCommit, Qt::DirectConnection);
    connect(m_workspace, &Workspace::currentChanged, this, &SessionWorkspace::syncModels);
    connect(m_workspace, &Workspace::currentIndexChanged, this, &SessionWorkspace::syncModels);
    connect(m_workspace, &Workspace::countChanged, this, &SessionWorkspace::syncModels);
    connect(m_workspace->models(), &QAbstractItemModel::rowsInserted, this, &SessionWorkspace::syncModels);
    connect(m_workspace->models(), &QAbstractItemModel::rowsRemoved, this, &SessionWorkspace::syncModels);
    connect(m_workspace->models(), &QAbstractItemModel::rowsMoved, this, &SessionWorkspace::syncModels);
    connect(m_root->outputModel(), &QAbstractItemModel::rowsInserted, this, &SessionWorkspace::syncOutputs);
    connect(m_root->outputModel(), &QAbstractItemModel::rowsRemoved, this, &SessionWorkspace::syncOutputs);
    connect(m_workspace->showOnAllWorkspaceModel(), &WorkspaceModel::surfaceAdded,
            this, &SessionWorkspace::syncModels);
    connect(m_workspace->showOnAllWorkspaceModel(), &WorkspaceModel::surfaceRemoved,
            this, &SessionWorkspace::syncModels);
    // attachInterface installs the owning socket before exposing the global.
    session()->attachInterface(m_protocol);
}

void SessionWorkspace::initializeProtocol()
{
    m_group = m_protocol->createGroup(EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE);
    syncOutputs();
    syncModels();
}

void SessionWorkspace::invalidateProtocol()
{
    m_handles.clear();
    m_outputs.clear();
    m_group = nullptr;
}

void SessionWorkspace::syncOutputs()
{
    if (!m_protocol || !m_group)
        return;
    QList<QPointer<WOutput>> outputs;
    for (auto *output : m_root->outputs()) {
        auto *native = output->output();
        connect(native, &WOutput::enabledChanged, this, &SessionWorkspace::syncOutputs, Qt::UniqueConnection);
        if (native->isEnabled())
            outputs.append(native);
    }
    for (const auto &output : std::as_const(m_outputs)) {
        if (output && !outputs.contains(output))
            m_protocol->outputLeave(m_group, output);
    }
    for (const auto &output : std::as_const(outputs)) {
        if (!m_outputs.contains(output))
            m_protocol->outputEnter(m_group, output);
    }
    m_outputs = outputs;
}

void SessionWorkspace::syncModels()
{
    if (!m_protocol || !m_group)
        return;
    for (auto it = m_handles.begin(); it != m_handles.end();) {
        if (!m_workspace->models()->objects().contains(it.key())) {
            m_protocol->removeWorkspace(it.value());
            it = m_handles.erase(it);
        } else {
            ++it;
        }
    }
    for (int index = 0; index < m_workspace->count(); ++index) {
        auto *model = m_workspace->modelAt(index);
        auto *handle = m_handles.value(model);
        if (!handle) {
            const auto id = m_protocolIdPrefix + ':' + QByteArray::number(model->id());
            handle = m_protocol->createWorkspace(id, EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE);
            if (!handle)
                continue;
            m_handles.insert(model, handle);
            m_protocol->setGroup(handle, m_group);
        }
        connect(model, &WorkspaceModel::nameChanged, this, &SessionWorkspace::syncModels, Qt::UniqueConnection);
        connect(model, &WorkspaceModel::surfaceAdded, this, &SessionWorkspace::syncModels, Qt::UniqueConnection);
        connect(model, &WorkspaceModel::surfaceRemoved, this, &SessionWorkspace::syncModels, Qt::UniqueConnection);
        auto surfaces = model->surfaces();
        surfaces.append(m_workspace->showOnAllWorkspaceModel()->surfaces());
        for (auto *surface : std::as_const(surfaces))
            connect(surface, &SurfaceWrapper::attentionChanged, this, &SessionWorkspace::syncModels, Qt::UniqueConnection);
        const bool urgent = std::ranges::any_of(surfaces, &SurfaceWrapper::attention);
        m_protocol->setName(handle, model->name());
        m_protocol->setCoordinates(handle, {static_cast<uint32_t>(index)});
        m_protocol->setState(handle, model == m_workspace->current(), urgent, false);
    }
}

void SessionWorkspace::handleCommit(wlr_ext_workspace_v1_commit_event *event)
{
    wlr_ext_workspace_v1_request *request, *next;
    wl_list_for_each_safe(request, next, event->requests, link) {
        switch (request->type) {
        case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE: {
            auto *model = m_handles.key(request->activate.workspace, nullptr);
            if (model)
                m_workspace->switchTo(m_workspace->models()->objects().indexOf(model), false);
            break;
        }
        case WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE: {
            if (request->create_workspace.group != m_group || !m_group
                || m_workspace->count() >= m_workspace->config()->maxWorkspace())
                break;
            const auto name = QString::fromUtf8(request->create_workspace.name);
            bool duplicate = false;
            for (auto *model : m_workspace->models()->objects())
                duplicate |= model->name() == name;
            if (duplicate)
                break;
            m_workspace->createModel(name);
            m_workspace->switchTo(m_workspace->count() - 1, false);
            break;
        }
        case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE:
        case WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN:
        case WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE:
            break; // Not advertised: Treeland has one workspace group spanning its outputs.
        }
    }
}
