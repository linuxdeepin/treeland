// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-output-group-manager-unstable-v1.h"

#include <QObject>
#include <QWaylandClientExtension>

class OutputGroupManager
    : public QWaylandClientExtensionTemplate<OutputGroupManager>
    , public QtWayland::treeland_output_group_manager_v1
{
    Q_OBJECT
public:
    explicit OutputGroupManager();
    ~OutputGroupManager() override;

    void instantiate();

    struct ::treeland_output_group_v1 *createGroup(const QString &sourceUuid);
    struct ::treeland_output_group_v1 *getGroup(const QString &name);

Q_SIGNALS:
    void groupAdded(const QString &name);
    void groupsDone();
    void groupRemoved(const QString &name);

protected:
    void treeland_output_group_manager_v1_group_added(const QString &name) override;
    void treeland_output_group_manager_v1_groups_done() override;
    void treeland_output_group_manager_v1_group_removed(const QString &name) override;
};
