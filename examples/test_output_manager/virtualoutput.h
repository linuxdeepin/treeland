// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-virtual-output-manager-v1.h"

#include <QObject>
#include <QStringList>

class VirtualOutput
    : public QObject
    , public QtWayland::treeland_virtual_output_v1
{
    Q_OBJECT
public:
    explicit VirtualOutput(struct ::treeland_virtual_output_v1 *object);
    ~VirtualOutput();

Q_SIGNALS:
    void outputsReceived(const QString &name, const QStringList &outputs);
    void errorOccurred(uint32_t code, const QString &message);

protected:
    void treeland_virtual_output_v1_outputs(const QString &name, wl_array *outputs) override;
    void treeland_virtual_output_v1_error(uint32_t code, const QString &message) override;
};
