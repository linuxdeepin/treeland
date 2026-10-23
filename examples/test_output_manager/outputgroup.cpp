// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "outputgroup.h"

#include <QDebug>

OutputGroup::OutputGroup(struct ::treeland_output_group_v1 *object)
    : QtWayland::treeland_output_group_v1(object)
{
}

OutputGroup::~OutputGroup()
{
    if (isInitialized())
        destroy();
}

void OutputGroup::requestSource(const QString &uuid)
{
    if (isInitialized())
        set_source(uuid);
}

void OutputGroup::requestAddMirror(const QString &uuid)
{
    if (isInitialized())
        add_output(uuid);
}

void OutputGroup::requestRemoveOutput(const QString &uuid)
{
    if (isInitialized())
        remove_output(uuid);
}

void OutputGroup::commit()
{
    if (isInitialized())
        QtWayland::treeland_output_group_v1::commit();
}

void OutputGroup::dissolve()
{
    if (isInitialized())
        QtWayland::treeland_output_group_v1::dissolve();
}

void OutputGroup::treeland_output_group_v1_name(const QString &name)
{
    m_name = name;
    Q_EMIT nameAssigned(name);
}

void OutputGroup::treeland_output_group_v1_done()
{
}

void OutputGroup::treeland_output_group_v1_state(const QString &source,
                                                 uint32_t count,
                                                 wl_array *uuids)
{
    QStringList mirrors;
    if (count > 0 && uuids && uuids->size >= static_cast<size_t>(count)) {
        const size_t length = uuids->size / count;
        const char *cursor = static_cast<const char *>(uuids->data);
        for (uint32_t i = 0; i < count; ++i) {
            mirrors << QString::fromUtf8(cursor, static_cast<int>(length));
            cursor += length;
        }
    }
    Q_EMIT stateChanged(source, mirrors);
}

void OutputGroup::treeland_output_group_v1_removed()
{
    Q_EMIT removed();
}

void OutputGroup::treeland_output_group_v1_operation_failed(uint32_t code, const QString &uuid)
{
    Q_EMIT operationFailed(code, uuid);
}
