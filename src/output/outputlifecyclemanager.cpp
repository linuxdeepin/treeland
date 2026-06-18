// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputlifecyclemanager.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "output.h"
#include "outputconfigstate.h"
#include "surface/surfacewrapper.h"
#include "wallpaper/wallpapermanager.h"

OutputLifecycleManager::OutputLifecycleManager(RootSurfaceContainer *rootContainer,
                                               OutputConfigState *configState,
                                               QObject *parent)
    : QObject(parent)
    , m_rootContainer(rootContainer)
    , m_configState(configState)
{
}

Output *OutputLifecycleManager::findFirstAvailableOutput(Output *excludeOutput)
{
    if (!m_rootContainer)
        return nullptr;

    const auto &outputs = m_rootContainer->outputs();
    for (auto output : std::as_const(outputs)) {
        if (output != excludeOutput && output->output() && output->output()->isEnabled()) {
            return output;
        }
    }
    return nullptr;
}

void OutputLifecycleManager::markScreenAsPrimaryIntent(Output *output)
{
    if (!output || !m_configState)
        return;

    QString name = output->output()->name();
    m_configState->markScreenAsPrimary(name);
}

void OutputLifecycleManager::restoreScreenAsPrimary(Output *output)
{
    if (!output || !m_rootContainer)
        return;

    m_rootContainer->setPrimaryOutput(output);
}

void OutputLifecycleManager::switchPrimaryOutput(Output *from,
                                                 Output *to,
                                                 const QList<SurfaceWrapper *> &surfaces)
{
    if (!m_rootContainer)
        return;

    m_rootContainer->setPrimaryOutput(to);
    m_rootContainer->moveSurfacesToOutput(surfaces, to, from);
}

void OutputLifecycleManager::recordSurfaceBindings(const QList<SurfaceWrapper *> &surfaces,
                                                     Output *sourceOutput)
{
    if (!m_configState || !sourceOutput)
        return;

    QString outputName = sourceOutput->output()->name();
    QRectF outputGeometry = sourceOutput->geometry();
    QPointF outputCenter = outputGeometry.center();
    QSizeF outputSize = outputGeometry.size();

    for (auto *surface : surfaces) {
        if (!surface)
            continue;

        auto surfaceType = surface->type();
        if (surfaceType != SurfaceWrapper::Type::XdgToplevel
            && surfaceType != SurfaceWrapper::Type::XWayland)
            continue;

        QPointF relativePos = surface->position() - outputCenter;
        int state = static_cast<int>(surface->surfaceState());
        m_configState->recordSurfaceBinding(surface, outputName, relativePos, outputSize, state);
    }
}

void OutputLifecycleManager::restoreSurfaceBindings(Output *targetOutput)
{
    if (!m_configState || !targetOutput || !m_rootContainer)
        return;

    QString outputName = targetOutput->output()->name();
    if (!m_configState->hasSurfaceBindings(outputName))
        return;

    QList<SurfaceBinding> bindings = m_configState->takeSurfaceBindings(outputName);

    QList<SurfaceBinding> validBindings;
    for (const auto &binding : bindings) {
        if (!binding.surface)
            continue;

        SurfaceWrapper *surface = binding.surface;
        auto surfaceType = surface->type();
        if (surfaceType != SurfaceWrapper::Type::XdgToplevel
            && surfaceType != SurfaceWrapper::Type::XWayland)
            continue;

        if (surface->ownsOutput() == targetOutput)
            continue;

        if (!surface->positionAutomatic())
            continue;

        validBindings.append(binding);
    }

    if (validBindings.isEmpty())
        return;

    QRectF targetGeometry = targetOutput->geometry();
    for (const auto &binding : validBindings) {
        SurfaceWrapper *surface = binding.surface;
        if (!surface)
            continue;

        SurfaceWrapper::State savedState = static_cast<SurfaceWrapper::State>(binding.surfaceState);

        if (savedState == SurfaceWrapper::State::Maximized
            || savedState == SurfaceWrapper::State::Fullscreen) {
            surface->setOwnsOutput(targetOutput);
            surface->setSurfaceState(savedState);
        } else if (savedState == SurfaceWrapper::State::Minimized) {
            surface->setOwnsOutput(targetOutput);
        } else {
            qreal scaleX = binding.originalOutputSize.width() > 0
                ? targetGeometry.width() / binding.originalOutputSize.width() : 1.0;
            qreal scaleY = binding.originalOutputSize.height() > 0
                ? targetGeometry.height() / binding.originalOutputSize.height() : 1.0;

            QPointF newPos(targetGeometry.center().x() + binding.relativePosition.x() * scaleX,
                           targetGeometry.center().y() + binding.relativePosition.y() * scaleY);

            const QSizeF size = surface->size();
            newPos.setX(
                qBound(targetGeometry.left(), newPos.x(), targetGeometry.right() - size.width()));
            newPos.setY(
                qBound(targetGeometry.top(), newPos.y(), targetGeometry.bottom() - size.height()));

            surface->setOwnsOutput(targetOutput);
            surface->setPosition(newPos);
        }
    }
}

void OutputLifecycleManager::onScreenAdded(Output *output)
{
    if (!output || !m_configState)
        return;

    QString outputName = output->output()->name();

    bool wasPrimary = m_configState->wasScreenPrimary(outputName);
    bool shouldRestoreCopy = m_configState->shouldRestoreCopyMode();
    bool hasPrimaryOutput = (m_rootContainer && m_rootContainer->primaryOutput() != nullptr);

    if (wasPrimary && m_mode == Mode::Extension && !shouldRestoreCopy && hasPrimaryOutput) {
        restoreScreenAsPrimary(output);
    }

    restoreSurfaceBindings(output);

    m_configState->clearOutputState(outputName);
}

void OutputLifecycleManager::onScreenRemoved(Output *output,
                                              const QList<SurfaceWrapper *> &surfaces,
                                              const QList<SurfaceWrapper *> &allWorkspacesSurfaces)
{
    if (!output || !m_rootContainer || !m_configState)
        return;

    QString outputName = output->output()->name();
    bool isCurrentPrimary = (m_rootContainer->primaryOutput() == output);
    bool wasPrimaryBeforeRemoval = m_configState->wasScreenPrimary(outputName);

    if (isCurrentPrimary && !wasPrimaryBeforeRemoval) {
        markScreenAsPrimaryIntent(output);
    }

    recordSurfaceBindings(allWorkspacesSurfaces, output);

    if (isCurrentPrimary || wasPrimaryBeforeRemoval) {
        auto newPrimary = m_rootContainer->primaryOutput();
        if (newPrimary && newPrimary != output) {
            m_rootContainer->moveSurfacesToOutput(surfaces, newPrimary, output);
        } else if (!m_rootContainer->outputs().isEmpty()) {
            Output *nextPrimary = findFirstAvailableOutput(output);
            if (nextPrimary) {
                m_rootContainer->setPrimaryOutput(nextPrimary);
                m_rootContainer->moveSurfacesToOutput(surfaces, nextPrimary, output);
            }
        }
    } else {
        auto primaryOutput = m_rootContainer->primaryOutput();
        if (primaryOutput) {
            m_rootContainer->moveSurfacesToOutput(surfaces, primaryOutput, output);
        }
    }
}

void OutputLifecycleManager::onScreenDisabled(Output *output,
                                               const QList<SurfaceWrapper *> &surfaces,
                                               const QList<SurfaceWrapper *> &allWorkspacesSurfaces)
{
    if (!output || !m_rootContainer)
        return;

    QString outputName = output->output()->name();
    bool isCurrentPrimary = (m_rootContainer->primaryOutput() == output);

    if (m_mode == Mode::Copy && isCurrentPrimary && m_configState) {
        m_configState->recordCopyModeExit();
    } else if (isCurrentPrimary && m_configState) {
        markScreenAsPrimaryIntent(output);
    }

    if (m_configState) {
        recordSurfaceBindings(allWorkspacesSurfaces, output);
    }

    if (isCurrentPrimary && !m_rootContainer->outputs().isEmpty()) {
        Output *nextPrimary = findFirstAvailableOutput(output);
        if (nextPrimary) {
            switchPrimaryOutput(output, nextPrimary, surfaces);
        }
    } else if (!isCurrentPrimary) {
        auto primaryOutput = m_rootContainer->primaryOutput();
        if (primaryOutput) {
            m_rootContainer->moveSurfacesToOutput(surfaces, primaryOutput, output);
        }
    }
}

void OutputLifecycleManager::onScreenEnabled(Output *output)
{
    if (!output || !m_configState || !m_rootContainer)
        return;

    QString outputName = output->output()->name();

    bool wasPrimary = m_configState->wasScreenPrimary(outputName);
    bool shouldRestoreCopy = m_configState->shouldRestoreCopyMode();

    if (m_mode == Mode::Extension && shouldRestoreCopy && m_rootContainer->outputs().size() >= 2) {
        m_configState->clearCopyModeIntent();
        m_copyModeRestoreIntent = true;
    } else if (wasPrimary && m_mode == Mode::Extension && !shouldRestoreCopy
             && m_rootContainer->primaryOutput()) {
        restoreScreenAsPrimary(output);
    }

    restoreSurfaceBindings(output);

    m_configState->clearOutputState(outputName);
}
