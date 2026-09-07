// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "layersurfacecontainer.h"

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "common/treelandlogging.h"

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
    // Only layer-surface is update it's outputs by `OutputLayerSurfaceContainer`
    // other types of surfaces are updated in `RootSurfaceContainer`.
    // Currently there is no consideration of layer surface spanning multiple outputs
    surface->setOutputs({m_output->output()});
    surface->setOwnsOutput(m_output);
}

void OutputLayerSurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    SurfaceContainer::removeSurface(surface);
    surface->setOutputs({});
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

    const auto surfaces = container->surfaces();
    for (SurfaceWrapper *surface : surfaces) {
        auto layerSurface = qobject_cast<WLayerSurface *>(surface->shellSurface());
        Q_ASSERT(layerSurface);
        // Needs to be moved to the new primary output
        if (!layerSurface->output() && rootContainer()->primaryOutput()) {
            container->removeSurface(surface);
            addSurfaceToContainer(surface);
        } else {
            layerSurface->closed();
        }
    }

    container->deleteLater();
}

void LayerSurfaceContainer::detachOutputSurfaces(Output *output)
{
    if (auto *container = getSurfaceContainer(output)) {
        // The surfaces are removed and immediately re-added containerless, so
        // listeners observing the layer model see a transient remove/re-add;
        // this is synchronous and no event loop runs in between.
        const auto surfaces = container->surfaces();
        for (SurfaceWrapper *surface : surfaces) {
            container->removeSurface(surface);
            // removeSurface() also dropped the surface from this layer's own
            // model and does not clear the QObject parent. Un-parent it here:
            // the old per-output container is deleteLater()ed and would
            // otherwise destroy a still-attached surface when the replacement
            // wrapper is never added (early return in a conversion path).
            // doAddSurface() re-parents to the new output container on
            // re-entry.
            unparentSurface(surface);
            doAddSurface(surface, false);
        }
    }
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
    surface->setHasInitializeContainer(true);
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
    surface->setHasInitializeContainer(false);
}

void LayerSurfaceContainer::addSurfaceToContainer(SurfaceWrapper *surface)
{
    Q_ASSERT(!surface->container());
    auto shell = qobject_cast<WLayerSurface *>(surface->shellSurface());
    auto output = shell->output()          ? shell->output()
        : rootContainer()->primaryOutput() ? rootContainer()->primaryOutput()->output()
                                           : nullptr;
    if (!output) {
        qCWarning(lcTlShell) << "No output, will close layer surface!";
        shell->closed();
        return;
    }
    auto container = getSurfaceContainer(output);
    if (!container) {
        // Can happen when the only remaining output was just removed and the
        // primary still points at it. Close the surface instead of crashing.
        qCWarning(lcTlShell) << "No layer surface container for output" << output->name()
                             << ", will close layer surface!";
        shell->closed();
        return;
    }
    Q_ASSERT(!container->surfaces().contains(surface));
    container->addSurface(surface);
}

void LayerSurfaceContainer::updateSurfacesContainer()
{
    for (SurfaceWrapper *surface : std::as_const(surfaces())) {
        if (!surface->container())
            addSurfaceToContainer(surface);
    }
}
