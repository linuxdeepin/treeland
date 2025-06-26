// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "virtualoutputmanager.h"

static VirtualOutputV1 *VIRTUAL_OUTPUT = nullptr;

VirtualOutputV1::VirtualOutputV1(QObject *parent)
    : QObject(parent)
{
    if (VIRTUAL_OUTPUT) {
        qFatal("There are multiple instances of VirtualOutputV1");
    }

    VIRTUAL_OUTPUT = this;
}

void VirtualOutputV1::onVirtualOutputCreated(treeland_virtual_output_v1 *virtual_output)
{
    m_virtualOutputv1.append(virtual_output);
    connect(virtual_output,
            &treeland_virtual_output_v1::before_destroy,
            this,
            [this, virtual_output] {
                m_virtualOutputv1.removeOne(virtual_output);
            });

    if (virtual_output->name.isEmpty()) {
        virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME,
                                   "Group name is empty!");
        return;
    }

    if (virtual_output->outputList.count() < 2) {
        virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_SCREEN_NUMBER,
                                   "The number of screens applying for copy mode is less than 2!");
        return;
    }

    Q_EMIT requestCreateVirtualOutput(virtual_output);
}

void VirtualOutputV1::onVirtualOutputDestroy(treeland_virtual_output_v1 *virtual_output)
{
    QList<treeland_virtual_output_v1 *>::iterator it = m_virtualOutputv1.begin();
    while (it != m_virtualOutputv1.end()) {
        if (*it == virtual_output) {
            it = m_virtualOutputv1.erase(it);
            Q_EMIT destroyVirtualOutput(virtual_output);
        } else {
            ++it;
        }
    }
}

QByteArrayView VirtualOutputV1::interfaceName() const
{
    return treeland_virtual_output_manager_v1_interface.name;
}

void VirtualOutputV1::create(WServer *server)
{
    m_manager = treeland_virtual_output_manager_v1::create(server->handle());
    connect(m_manager,
            &treeland_virtual_output_manager_v1::virtualOutputCreated,
            this,
            &VirtualOutputV1::onVirtualOutputCreated);
    connect(m_manager,
            &treeland_virtual_output_manager_v1::virtualOutputDestroy,
            this,
            &VirtualOutputV1::onVirtualOutputDestroy);
}

void VirtualOutputV1::destroy([[maybe_unused]] WServer *server) { }

wl_global *VirtualOutputV1::global() const
{
    return m_manager->global;
}
