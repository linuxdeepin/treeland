// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputlifecyclemanager.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "output.h"
#include "outputconfigstate.h"

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

void OutputLifecycleManager::migrateSurfacesToNewPrimary(Output *removedOutput,
                                                         const QList<SurfaceWrapper *> &surfaces)
{
    // NOTE: This must be called while removedOutput's Output item still has valid position
    // data. The call chain is: Helper::onOutputRemoved -> removeOutput (switches primary)
    // -> onScreenRemoved -> migrateSurfacesToNewPrimary. At this point the removed output
    // has been removed from the layout, but the Output object (and its item position) still
    // exists because `delete o` happens after onScreenRemoved returns.
    if (!m_rootContainer || surfaces.isEmpty())
        return;

    auto newPrimary = m_rootContainer->primaryOutput();
    if (newPrimary) {
        m_rootContainer->moveSurfacesToOutput(surfaces, newPrimary, removedOutput);
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

    m_configState->clearOutputState(outputName);
}

void OutputLifecycleManager::onScreenRemoved(Output *output,
                                             const QList<SurfaceWrapper *> &surfaces)
{
    if (!output || !m_rootContainer || !m_configState)
        return;

    QString outputName = output->output()->name();
    bool isCurrentPrimary = (m_rootContainer->primaryOutput() == output);
    bool wasPrimaryBeforeRemoval = m_configState->wasScreenPrimary(outputName);

    if (isCurrentPrimary && !wasPrimaryBeforeRemoval) {
        markScreenAsPrimaryIntent(output);
    }

    if (!isCurrentPrimary) {
        migrateSurfacesToNewPrimary(output, surfaces);
    }
}

void OutputLifecycleManager::onScreenDisabled(Output *output,
                                              const QList<SurfaceWrapper *> &surfaces)
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

    if (isCurrentPrimary && !m_rootContainer->outputs().isEmpty()) {
        Output *nextPrimary = findFirstAvailableOutput(output);
        if (nextPrimary) {
            switchPrimaryOutput(output, nextPrimary, surfaces);
        }
    } else if (!isCurrentPrimary) {
        migrateSurfacesToNewPrimary(output, surfaces);
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

    m_configState->clearOutputState(outputName);
}
