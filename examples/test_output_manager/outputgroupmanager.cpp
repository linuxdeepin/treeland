// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "outputgroupmanager.h"

OutputGroupManager::OutputGroupManager()
    : QWaylandClientExtensionTemplate<OutputGroupManager>(1)
{
}

OutputGroupManager::~OutputGroupManager()
{
    if (isInitialized())
        destroy();
}

void OutputGroupManager::instantiate()
{
    initialize();
}

struct ::treeland_output_group_v1 *OutputGroupManager::createGroup(const QString &sourceUuid)
{
    if (!isInitialized())
        return nullptr;
    return create_group(sourceUuid);
}

struct ::treeland_output_group_v1 *OutputGroupManager::getGroup(const QString &name)
{
    if (!isInitialized())
        return nullptr;
    return get_group(name);
}

void OutputGroupManager::treeland_output_group_manager_v1_group_added(const QString &name)
{
    Q_EMIT groupAdded(name);
}

void OutputGroupManager::treeland_output_group_manager_v1_groups_done()
{
    Q_EMIT groupsDone();
}

void OutputGroupManager::treeland_output_group_manager_v1_group_removed(const QString &name)
{
    Q_EMIT groupRemoved(name);
}
