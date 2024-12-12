// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"

#include <wglobal.h>

WAYLIB_SERVER_USE_NAMESPACE

class LayerSurfaceContainer;

class OutputLayerSurfaceContainer : public SurfaceContainer
{
public:
    explicit OutputLayerSurfaceContainer(Output *output, LayerSurfaceContainer *parent);

    Output *output() const;

    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

private:
    Output *m_output;
};

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

class LayerSurfaceContainer : public SurfaceContainer
{
public:
    explicit LayerSurfaceContainer(SurfaceContainer *parent);

    void addOutput(Output *output) override;
    void removeOutput(Output *output) override;
    OutputLayerSurfaceContainer *getSurfaceContainer(const Output *output) const;
    OutputLayerSurfaceContainer *getSurfaceContainer(const WOutput *output) const;

    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

private:
    void addSurfaceToContainer(SurfaceWrapper *surface);
    void updateSurfacesContainer();

    QList<OutputLayerSurfaceContainer *> m_surfaceContainers;
};
