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
    WOutputViewport *outputviewport, WQuickTextureProxy *proxy, VirtualOutputV1 *virtualOutput)
    : QObject(outputviewport)
    , m_viewport(outputviewport)
    , m_backviewport(outputviewport)
    , m_textureproxy(proxy)
    , m_virtualoutput(virtualOutput)
{
    m_virtualoutput->m_viewports_list << qMakePair(outputviewport, proxy);
    connect(m_virtualoutput,
            &VirtualOutputV1::requestCreateVirtualOutput,
            this,
            [this](QString name, QStringList outputList) {

                if (m_virtualoutput->m_renderWindow) {
                    m_virtualoutput->m_renderWindow = qobject_cast<WOutputRenderWindow*>(m_viewport->window());
                }

                if (outputList.at(0) == m_viewport->output()->name()) {
                    m_virtualoutput->m_hardwareLayersOfPrimaryOutput = m_viewport->hardwareLayers();
                    connect(m_viewport, &WOutputViewport::hardwareLayersChanged,
                            this, &VirtualOutputManagerAttached::updatePrimaryOutputHardwareLayers);
                }

                if (outputList.contains(m_viewport->output()->name()) && m_viewport->output()->name() != outputList.at(0)) {
                    removeItem(m_viewport);
                    copyScreen(outputList);
                }
            });

    connect(m_virtualoutput,
            &VirtualOutputV1::destroyVirtualOutput,
            this,
            [this](QString name, QStringList outputList) {
                if (outputList.contains(m_backviewport->output()->name()) && m_backviewport->output()->name() != outputList.at(0)) {
                    addItem(m_viewport, m_backviewport);
                    restoreScreen(outputList);
                }

            });

    connect(m_virtualoutput,
            &VirtualOutputV1::removeVirtualOutput,
            this,
            [this](QStringList outputList, WOutput *output) {
                if (m_viewport == m_backviewport)
                    return;
                addItem(m_viewport, m_backviewport);
                restoreScreen(outputList);

            });
    connect(m_virtualoutput,
            &VirtualOutputV1::newVirtualOutput,
            this,
            [this](QStringList outputList, WOutput *output) {
                if (outputList.contains(output->name()) && m_viewport->output()->name() != outputList.at(0)) {
                    removeItem(m_viewport);
                    copyScreen(outputList);
                }
            });
}

void VirtualOutputManagerAttached::removeItem(WOutputViewport *viewport)
{
    WQuickOutputLayout *outputlayout = static_cast<WQuickOutputLayout*>(viewport->output()->layout());
    WOutputItem *item = WOutputItem::getOutputItem(viewport->output());
    outputlayout->remove(item);
}

void VirtualOutputManagerAttached::addItem(WOutputViewport *viewport, WOutputViewport *backviewport)
{
    WQuickOutputLayout *outputlayout = static_cast<WQuickOutputLayout*>(viewport->output()->layout());
    WOutputItem *item = WOutputItem::getOutputItem(backviewport->output());
    outputlayout->add(item);
}

void VirtualOutputManagerAttached::copyScreen(QStringList outputList)
{
    for (auto outputs : std::as_const(m_virtualoutput->m_viewports_list)) {

        auto outputviewport = outputs.first;
        if (outputviewport->output()->name() == outputList.at(0)) {
            for (auto layer : std::as_const(m_virtualoutput->m_hardwareLayersOfPrimaryOutput))
                m_virtualoutput->m_renderWindow->attach(layer, m_viewport, outputviewport, m_textureproxy);

            outputviewport->setCacheBuffer(true);

            m_viewport = outputviewport;

            Q_EMIT outputViewportChanged();
        }
    }
}

void VirtualOutputManagerAttached::restoreScreen(QStringList outputList)
{
    for (auto outputs : m_virtualoutput->m_viewports_list) {
        auto outputviewport = outputs.first;
        if (outputviewport->output()->name() == outputList.at(0)) {
            for (auto layer : std::as_const(m_virtualoutput->m_hardwareLayersOfPrimaryOutput))
                m_virtualoutput->m_renderWindow->detach(layer, m_backviewport);

            outputviewport->setCacheBuffer(false);

            m_viewport = m_backviewport;
            Q_EMIT outputViewportChanged();
        }
    }
}

void VirtualOutputManagerAttached::updatePrimaryOutputHardwareLayers()
{
    const auto layers = m_viewport->hardwareLayers();
    for (auto layer : layers) {
        if (m_virtualoutput->m_hardwareLayersOfPrimaryOutput.removeOne(layer))
            continue;

        for (auto outputs : m_virtualoutput->m_viewports_list) {
            if (outputs.first != m_viewport) {
                m_virtualoutput->m_renderWindow->attach(layer, outputs.first, m_viewport, outputs.second);
            }
        }

    }

    for (auto oldLayer : std::as_const(m_virtualoutput->m_hardwareLayersOfPrimaryOutput)) {
        for (auto copyOutput : std::as_const(m_virtualoutput->m_viewports_list))
            m_virtualoutput->m_renderWindow->detach(oldLayer, copyOutput.first);
    }

    m_virtualoutput->m_hardwareLayersOfPrimaryOutput = layers;
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
        virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_GROUP_NAME,"Group name is empty!");
        return;
    }

    if (virtual_output->outputList.count() < 2) {
        virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_SCREEN_NUMBER,"The number of screens applying for copy mode is less than 2!");
        return;
    } else {
        QStringList outputList;

        for (auto outputviewport : m_viewports_list) {
            outputList.append(outputviewport.first->output()->name());
        }

        for (const QString& output : virtual_output->outputList) {
            if (!outputList.contains(output)) {
                QString screen = output + " does not exist!";
                virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,screen.toLocal8Bit().data());
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

VirtualOutputManagerAttached *VirtualOutputV1::Attach(WOutputViewport *outputviewport, WQuickTextureProxy *proxy)
{
    if (outputviewport) {
        return new VirtualOutputManagerAttached(outputviewport, proxy, VIRTUAL_OUTPUT);
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

    for (int i = 0; i < m_viewports_list.size(); ++i) {
        WOutputViewport *viewport = m_viewports_list[i].first;
        if (viewport->output() == output) {
            m_viewports_list.removeAt(i);
            break;
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

void VirtualOutputV1::destroy(WServer *server) { }

wl_global *VirtualOutputV1::global() const
{
    return m_manager->global;
}
