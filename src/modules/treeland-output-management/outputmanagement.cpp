// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputmanagement.h"

#include "impl/output_manager_impl.h"
#include "wglobal.h"

#include <woutput.h>
#include <wserver.h>

#include <qwdisplay.h>

#include <QDebug>
#include <QQmlInfo>

extern "C" {
#include <wayland-server-core.h>
}

TreelandOutputManager::TreelandOutputManager(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{
}

const char *TreelandOutputManager::primaryOutput()
{
    return m_handle->primary_output_name;
}

bool TreelandOutputManager::setPrimaryOutput(const char *name)
{
    if (name == nullptr) {
        if (m_outputs.empty()) { // allow null when output list is empty
            m_handle->set_primary_output(nullptr);
            Q_EMIT primaryOutputChanged();
            return true;
        } else {
            return false;
        }
    }
    if (m_handle->primary_output_name != nullptr
        && strcmp(m_handle->primary_output_name, name) == 0)
        return true;
    for (auto *output : m_outputs)
        if (strcmp(output->nativeHandle()->name, name) == 0) {
            m_handle->set_primary_output(output->nativeHandle()->name);
            Q_EMIT primaryOutputChanged();
            return true;
        }
    qmlWarning(this) << QString("Try to set unknown output(%1) as primary output!").arg(name);
    return false;
}

void TreelandOutputManager::newOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    m_outputs.append(output);
    if (m_handle->primary_output_name == nullptr)
        setPrimaryOutput(output->nativeHandle()->name);
}

void TreelandOutputManager::removeOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    m_outputs.removeOne(output);

    if (m_handle->primary_output_name == output->nativeHandle()->name) {
        if (m_outputs.isEmpty()) {
            setPrimaryOutput(nullptr);
        } else {
            setPrimaryOutput(m_outputs.first()->nativeHandle()->name);
        }
    }
}

WServerInterface *TreelandOutputManager::create()
{
    m_handle = treeland_output_manager_v1::create(server()->handle());

    connect(m_handle,
            &treeland_output_manager_v1::requestSetPrimaryOutput,
            this,
            &TreelandOutputManager::requestSetPrimaryOutput);
    return new WServerInterface(m_handle, m_handle->global);
}
