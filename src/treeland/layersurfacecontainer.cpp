// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "layersurfacecontainer.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"

#include <wlayersurface.h>
#include <woutputitem.h>

WAYLIB_SERVER_USE_NAMESPACE

OutputLayerSurfaceContainer::OutputLayerSurfaceContainer(Output *output,
                                                         LayerSurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_output(output)
{
}

Output *OutputLayerSurfaceContainer::output() const
{
    return m_output;
}

void OutputLayerSurfaceContainer::addSurface(SurfaceWrapper *surface)
{
    SurfaceContainer::addSurface(surface);
    surface->setOwnsOutput(m_output);
}

void OutputLayerSurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    SurfaceContainer::removeSurface(surface);
    if (surface->ownsOutput() == m_output)
        surface->setOwnsOutput(nullptr);
}

LayerSurfaceContainer::LayerSurfaceContainer(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{
}

void LayerSurfaceContainer::addOutput(Output *output)
{
    Q_ASSERT(!getSurfaceContainer(output));
    auto container = new OutputLayerSurfaceContainer(output, this);
    m_surfaceContainers.append(container);
    updateSurfacesContainer();
}

void LayerSurfaceContainer::removeOutput(Output *output)
{
    OutputLayerSurfaceContainer *container = getSurfaceContainer(output);
    Q_ASSERT(container);
    m_surfaceContainers.removeOne(container);

    for (SurfaceWrapper *surface : container->surfaces()) {
        container->removeSurface(surface);
        auto layerSurface = qobject_cast<WLayerSurface *>(surface->shellSurface());
        Q_ASSERT(layerSurface);
        // Needs to be moved to the new primary output
        if (!layerSurface->output())
            addSurfaceToContainer(surface);
    }

    container->deleteLater();
}

OutputLayerSurfaceContainer *LayerSurfaceContainer::getSurfaceContainer(const Output *output) const
{
    for (OutputLayerSurfaceContainer *container : std::as_const(m_surfaceContainers)) {
        if (container->output() == output)
            return container;
    }
    return nullptr;
}

OutputLayerSurfaceContainer *LayerSurfaceContainer::getSurfaceContainer(const WOutput *output) const
{
    for (OutputLayerSurfaceContainer *container : std::as_const(m_surfaceContainers)) {
        if (container->output()->output() == output)
            return container;
    }
    return nullptr;
}

void LayerSurfaceContainer::addSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(surface->type() == SurfaceWrapper::Type::Layer);
    if (!SurfaceContainer::doAddSurface(surface, false))
        return;
    addSurfaceToContainer(surface);
}

void LayerSurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    if (!SurfaceContainer::doRemoveSurface(surface, false))
        return;
    auto shell = qobject_cast<WLayerSurface *>(surface->shellSurface());
    auto output = shell->output();
    auto container = getSurfaceContainer(output);
    Q_ASSERT(container);
    Q_ASSERT(container->surfaces().contains(surface));
    container->removeSurface(surface);
}

void LayerSurfaceContainer::addSurfaceToContainer(SurfaceWrapper *surface)
{
    Q_ASSERT(!surface->container());
    auto shell = qobject_cast<WLayerSurface *>(surface->shellSurface());
    auto output = shell->output() ? shell->output() : rootContainer()->primaryOutput()->output();
    if (!output) {
        qCWarning(qLcLayerShell) << "No output, will close layer surface!";
        shell->closed();
        return;
    }
    auto container = getSurfaceContainer(output);
    Q_ASSERT(container);
    Q_ASSERT(!container->surfaces().contains(surface));
    container->addSurface(surface);
}

void LayerSurfaceContainer::updateSurfacesContainer()
{
    for (SurfaceWrapper *surface : surfaces()) {
        if (!surface->container())
            addSurfaceToContainer(surface);
    }
}
