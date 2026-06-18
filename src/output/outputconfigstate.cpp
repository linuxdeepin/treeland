// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputconfigstate.h"
#include "surface/surfacewrapper.h"

void OutputConfigState::markScreenAsPrimary(const QString &outputName)
{
    if (m_states.contains(outputName)) {
        m_states[outputName].wasPrimary = true;
    } else {
        m_states[outputName] = OutputPrimaryState{true};
    }
}

bool OutputConfigState::wasScreenPrimary(const QString &outputName) const
{
    if (m_states.contains(outputName)) {
        return m_states[outputName].wasPrimary;
    }
    return false;
}

void OutputConfigState::clearOutputState(const QString &outputName)
{
    m_states.remove(outputName);
}

void OutputConfigState::recordCopyModeExit()
{
    m_copyModeExited = true;
}

bool OutputConfigState::shouldRestoreCopyMode() const
{
    return m_copyModeExited;
}

void OutputConfigState::clearCopyModeIntent()
{
    m_copyModeExited = false;
}

void OutputConfigState::recordSurfaceBinding(SurfaceWrapper *surface,
                                               const QString &originalOutputName,
                                               const QPointF &relativePosition,
                                               const QSizeF &originalOutputSize,
                                               int surfaceState)
{
    if (!surface)
        return;

    auto &bindings = m_surfaceBindings[originalOutputName];
    for (int i = 0; i < bindings.size(); ++i) {
        if (bindings[i].surface == surface) {
            bindings[i].relativePosition = relativePosition;
            bindings[i].originalOutputSize = originalOutputSize;
            bindings[i].surfaceState = surfaceState;
            return;
        }
    }

    SurfaceBinding binding;
    binding.surface = surface;
    binding.relativePosition = relativePosition;
    binding.originalOutputSize = originalOutputSize;
    binding.surfaceState = surfaceState;

    bindings.append(binding);
}

QList<SurfaceBinding> OutputConfigState::takeSurfaceBindings(const QString &outputName)
{
    return m_surfaceBindings.take(outputName);
}

bool OutputConfigState::hasSurfaceBindings(const QString &outputName) const
{
    auto it = m_surfaceBindings.constFind(outputName);
    return it != m_surfaceBindings.constEnd() && !it->isEmpty();
}
