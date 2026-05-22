// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-virtual-output-manager-v1.h"

#include <QObject>
#include <QStringList>
#include <QWaylandClientExtension>

class VirtualOutputManager
    : public QWaylandClientExtensionTemplate<VirtualOutputManager>
    , public QtWayland::treeland_virtual_output_manager_v1
{
    Q_OBJECT
public:
    explicit VirtualOutputManager();
    ~VirtualOutputManager();

    void instantiate();

    struct ::treeland_virtual_output_v1 *createVirtualOutput(const QString &name,
                                                             const QByteArray &outputs);
    void getVirtualOutputList();
    struct ::treeland_virtual_output_v1 *getVirtualOutput(const QString &name);

Q_SIGNALS:
    void virtualOutputListReceived(const QStringList &names);

protected:
    void treeland_virtual_output_manager_v1_virtual_output_list(wl_array *names) override;
};
