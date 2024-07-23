// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "impl/virtual_output_manager_impl.h"

#include "virtualoutputmanager.h"

#include <wserver.h>

#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <woutput.h>
#include <woutputitem.h>
#include <wquickoutputlayout.h>
#include <QQmlInfo>

extern "C" {
#include <wayland-server-core.h>
}

static VirtualOutputV1 *VIRTUAL_OUTPUT = nullptr;

VirtualOutputManagerAttached::VirtualOutputManagerAttached(
    WOutputViewport *outputviewport, VirtualOutputV1 *virtualOutput)
    : QObject(outputviewport)
    , m_viewport(outputviewport)
    , m_backviewport(outputviewport)
    , m_virtualoutput(virtualOutput)
{
    m_virtualoutput->m_viewports_list.append(outputviewport);
    connect(m_virtualoutput,
            &VirtualOutputV1::requestCreateVirtualOutput,
            this,
            [this](QString name, QStringList outputList) {
                if (outputList.contains(m_viewport->output()->name()) && m_viewport->output()->name() != outputList.at(0)) {
                    // todo: Replication mode Enable the soft cursor and wait for the hard cursor to be reconstructed
                    m_virtualoutput->m_viewports_list.at(0)->output()->setForceSoftwareCursor(true);

                    WQuickOutputLayout *outputlayout = static_cast<WQuickOutputLayout*>(m_viewport->output()->layout());
                    WOutputItem *item = WOutputItem::getOutputItem(m_viewport->output());
                    outputlayout->remove(item);

                    m_viewport = m_virtualoutput->m_viewports_list.at(0);

                    Q_EMIT outputViewportChanged();
                }

            });

    connect(m_virtualoutput,
            &VirtualOutputV1::destroyVirtualOutput,
            this,
            [this](QString name, QStringList outputList) {
                if (outputList.contains(m_backviewport->output()->name()) && m_backviewport->output()->name() != outputList.at(0)) {
                    m_viewport->output()->setForceSoftwareCursor(false);

                    WQuickOutputLayout *outputlayout = static_cast<WQuickOutputLayout*>(m_viewport->output()->layout());
                    WOutputItem *item = WOutputItem::getOutputItem(m_backviewport->output());
                    outputlayout->add(item);
                    m_viewport = m_backviewport;

                    Q_EMIT outputViewportChanged();
                }

            });

    connect(m_virtualoutput,
            &VirtualOutputV1::removeVirtualOutput,
            this,
            [this](QStringList outputList, WOutput *output) {
                if (output->name() != m_backviewport->output()->name() && m_viewport == m_backviewport) {
                    if (m_viewport == m_backviewport)
                        return;

                    m_viewport = m_backviewport;
                    m_viewport->setInput(nullptr);
                    Q_EMIT outputViewportChanged();
                }
            });
    connect(m_virtualoutput,
            &VirtualOutputV1::newVirtualOutput,
            this,
            [this](QStringList outputList, WOutput *output) {
                if (m_viewport->output() == output) {
                    if (m_viewport == m_virtualoutput->m_viewports_list.at(0))
                        return;
                    m_virtualoutput->m_viewports_list.at(0)->output()->setForceSoftwareCursor(true);

                    WQuickOutputLayout *outputlayout = static_cast<WQuickOutputLayout*>(m_viewport->output()->layout());
                    WOutputItem *item = WOutputItem::getOutputItem(m_viewport->output());
                    outputlayout->remove(item);

                    m_viewport = m_virtualoutput->m_viewports_list.at(0);
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
    m_virtualOutputv1.append(virtual_output);
    connect(virtual_output, &treeland_virtual_output_v1::before_destroy, this, [this, virtual_output] {
        m_virtualOutputv1.removeOne(virtual_output);
    });

    if (virtual_output->name.isEmpty()) {
        virtual_output->send_error(VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME,"Group name is empty!");
        return;
    }

    if (virtual_output->outputList.count() < 2) {
        virtual_output->send_error(VIRTUAL_OUTPUT_V1_ERROR_INVALID_SCREEN_NUMBER,"The number of screens applying for copy mode is less than 2!");
        return;
    } else {
        QStringList outputList;
        for (auto *outputviewport : m_viewports_list) {
            outputList.append(outputviewport->output()->name());
        }

        for (const QString& output : virtual_output->outputList) {
            if (!outputList.contains(output)) {
                QString screen = output + " does not exist!";
                virtual_output->send_error(VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,screen.toLocal8Bit().data());
                return;
            }
        }
    }

    Q_EMIT requestCreateVirtualOutput(virtual_output->name, virtual_output->outputList);
}

void VirtualOutputV1::onVirtualOutputDestroy(treeland_virtual_output_v1 *virtual_output)
{
    QList<treeland_virtual_output_v1 *>::iterator it = m_virtualOutputv1.begin();
    while (it != m_virtualOutputv1.end()) {
        if (*it == virtual_output) {
            it = m_virtualOutputv1.erase(it);
            Q_EMIT destroyVirtualOutput(virtual_output->name, virtual_output->outputList);
        } else {
            ++it;
        }
    }
}

VirtualOutputManagerAttached *VirtualOutputV1::Attach(WOutputViewport *outputviewport)
{
    if (outputviewport) {
        return new VirtualOutputManagerAttached(outputviewport, VIRTUAL_OUTPUT);
    }

    return nullptr;
}

void VirtualOutputV1::setVirtualOutput(QString name, QStringList outputList)
{
    if (name.isEmpty()) {
        return;
    }

    Q_EMIT requestCreateVirtualOutput(name, outputList);
}

void VirtualOutputV1::newOutput(WOutput *output)
{
    if (!m_manager) {
        return;
    }

    for (auto *virtualOutput : m_virtualOutputv1) {
        if (virtualOutput->outputList.contains(output->name())){
            Q_EMIT newVirtualOutput(virtualOutput->outputList, output);
        }
    }
}

void VirtualOutputV1::removeOutput(WOutput *output)
{
    if (!m_manager) {
        return;
    }

    for (auto *virtualOutput : m_virtualOutputv1) {
        if (virtualOutput->outputList.contains(output->name())){
            Q_EMIT removeVirtualOutput(virtualOutput->outputList, output);
        }
    }

    for (auto *outputviewport : m_viewports_list) {
        if (outputviewport->output() == output){
            m_viewports_list.removeOne(outputviewport);
        }
    }
}

QByteArrayView VirtualOutputV1::interfaceName() const
{
    return virtual_output_manager_v1_interface.name;
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

void VirtualOutputV1::destroy(WServer *server) { }

wl_global *VirtualOutputV1::global() const
{
    return m_manager->global;
}
