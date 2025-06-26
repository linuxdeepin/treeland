// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputmanagement.h"

#include "modules/primary-output/impl/output_manager_impl.h"

PrimaryOutputV1::PrimaryOutputV1(QObject *parent)
    : QObject(parent)
{
}

void PrimaryOutputV1::sendPrimaryOutput(const char *name)
{
    m_handle->set_primary_output(name);
}

void PrimaryOutputV1::create(WServer *server)
{
    m_handle = treeland_output_manager_v1::create(server->handle());

    connect(m_handle,
            &treeland_output_manager_v1::requestSetPrimaryOutput,
            this,
            &PrimaryOutputV1::requestSetPrimaryOutput);
}

void PrimaryOutputV1::destroy([[maybe_unused]] WServer *server) { }

wl_global *PrimaryOutputV1::global() const
{
    return m_handle->global;
}

QByteArrayView PrimaryOutputV1::interfaceName() const
{
    return treeland_output_manager_v1_interface.name;
}
