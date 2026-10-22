// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-treeland-output-group-manager-unstable-v1.h"

#include <QObject>
#include <QString>

class OutputGroup
    : public QObject
    , public QtWayland::treeland_output_group_v1
{
    Q_OBJECT
public:
    explicit OutputGroup(struct ::treeland_output_group_v1 *object);
    ~OutputGroup() override;

    QString name() const { return m_name; }
    bool isValid() const { return m_valid; }

    void requestSource(const QString &uuid);
    void requestAddMirror(const QString &uuid);
    void requestRemoveOutput(const QString &uuid);
    void commit();
    void dissolve();

Q_SIGNALS:
    void nameAssigned(const QString &name);
    void stateChanged(const QString &sourceUuid, const QStringList &mirrorUuids);
    void removed();
    void operationFailed(uint32_t code, const QString &uuid);

protected:
    void treeland_output_group_v1_name(const QString &name) override;
    void treeland_output_group_v1_done() override;
    void treeland_output_group_v1_state(const QString &source,
                                        uint32_t count,
                                        wl_array *uuids) override;
    void treeland_output_group_v1_removed() override;
    void treeland_output_group_v1_operation_failed(uint32_t code, const QString &uuid) override;

private:
    QString m_name;
    bool m_valid = true;
};
