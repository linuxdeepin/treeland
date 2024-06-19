// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "impl/virtual_output_manager_impl.h"

#include "virtualoutputmanager.h"

#include <wserver.h>

#include <qwdisplay.h>
#include <qwsignalconnector.h>

#include <woutput.h>
#include <wxdgsurface.h>
#include <wxwaylandsurface.h>

#include <QDebug>
#include <QQmlInfo>

extern "C" {
#include <wayland-server-core.h>
}

static VirtualOutputV1 *VIRTUAL_OUTPUT = nullptr;



VirtualOutputManagerAttached::VirtualOutputManagerAttached(
    WOutputViewport *outputviewport, VirtualOutputV1 *manager)
    : QObject(outputviewport)
    , m_manager(manager)
{

    m_viewport = outputviewport;
    m_manager->m_viewports_list.append(outputviewport);

    connect(m_manager,
            &VirtualOutputV1::requestCreateVirtualOutput,
            this,
            [this](QString name, QStringList outputList) {

                if(outputList.at(1) == m_viewport->output()->name()) {
                    m_viewport = m_manager->m_viewports_list.at(0);

                    qWarning() << "------------------------------" << name;
                    Q_EMIT outputViewportChanged();
                }

            });
}

VirtualOutputV1::VirtualOutputV1(QObject *parent)
{
    if (VIRTUAL_OUTPUT) {
        qFatal("There are multiple instances of VirtualOutputV1");
    }

    VIRTUAL_OUTPUT = this;
}

void VirtualOutputV1::onVirtualOutputCreated(treeland_virtual_output_v1 *virtual_output)
{
    m_virtualOutput.push_back(virtual_output);
    connect(virtual_output, &treeland_virtual_output_v1::beforeDestroy, this, [this, virtual_output] {
        std::erase_if(m_virtualOutput, [virtual_output](auto *p) {
            return p == virtual_output;
        });
    });

    //virtual_output->outputList.count() >= 2; virtual_output->outputList.at(i) must be in outputviewport->output()
    if(virtual_output->name.isEmpty() && !virtual_output->screen_outputs)
        return; // todo: send error to client

    Q_EMIT requestCreateVirtualOutput(virtual_output->name, virtual_output->outputList);
}

VirtualOutputManagerAttached *VirtualOutputV1::Attach(WOutputViewport *outputviewport)
{
    if (outputviewport) {
        return new VirtualOutputManagerAttached(outputviewport,
                                         VIRTUAL_OUTPUT);
    }

    return nullptr;
}

void VirtualOutputV1::setVirtualOutput(QString name, QStringList outputList)
{
    if(name.isEmpty()) {
        return;
    }

    Q_EMIT requestCreateVirtualOutput(name, outputList);
}

void VirtualOutputV1::create(WServer *server)
{
    m_manager = treeland_virtual_output_manager_v1::create(server->handle());
    connect(m_manager,
            &treeland_virtual_output_manager_v1::virtualOutputCreated,
            this,
            &VirtualOutputV1::onVirtualOutputCreated);
}

void VirtualOutputV1::destroy(WServer *server) { }

wl_global *VirtualOutputV1::global() const
{
    return m_manager->global;
}
