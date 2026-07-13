// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputmanager.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "output.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "wallpaper/wallpapermanager.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

#include <utility>

#include <qwoutput.h>

namespace {
QString serializeOutputIds(const QStringList &outputs)
{
    QJsonArray array;
    for (const auto &output : std::as_const(outputs)) {
        array.append(output);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QStringList deserializeOutputIds(const QString &value)
{
    const auto document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray()) {
        return {};
    }

    const auto array = document.array();
    QStringList outputs;
    for (const auto &entry : std::as_const(array)) {
        if (entry.isString()) {
            outputs.append(entry.toString());
        }
    }
    return outputs;
}
}

OutputManager::OutputManager(RootSurfaceContainer *rootContainer,
                             TreelandConfig *config,
                             QObject *parent)
    : QObject(parent)
    , m_rootContainer(rootContainer)
    , m_config(config)
{
}

void OutputManager::setMode(Mode mode)
{
    m_mode = mode;
}

OutputManager::Mode OutputManager::mode() const
{
    return m_mode;
}

bool OutputManager::shouldRestoreCopyMode(int availableOutputCount) const
{
    return m_config && m_config->singleOutputId().isEmpty()
        && m_config->createCopyOutput() && availableOutputCount >= 2;
}

bool OutputManager::restoreConfiguredSingleOutput(const QList<SurfaceWrapper *> &surfaces, bool updatePrimaryOutputConfig)
{
    if (!m_config || !m_rootContainer || m_config->singleOutputId().isEmpty()) {
        return false;
    }

    Output *singleOutput = findOutputById(m_config->singleOutputId());
    if (!singleOutput) {
        qCWarning(lcTlOutput) << "Cannot restore single-output display: output is not available"
                              << m_config->singleOutputId();
        return false;
    }

    for (auto *output : std::as_const(m_rootContainer->outputs())) {
        if (!output || !output->output()) {
            continue;
        }
        if (output == singleOutput) {
            if (!output->output()->isEnabled()) {
                output->enable();
            }
            continue;
        }
        if (output->output()->isEnabled()) {
            struct wlr_output_state state;
            wlr_output_state_init(&state);
            wlr_output_state_set_enabled(&state, false);
            const bool committed = wlr_output_commit_state(output->output()->nativeHandle(), &state);
            wlr_output_state_finish(&state);
            if (!committed) {
                qCWarning(lcTlOutput) << "Failed to disable non-selected output while restoring single-output display"
                                      << output->output()->name();
            }
        }
    }

    m_rootContainer->setPrimaryOutput(singleOutput, updatePrimaryOutputConfig);
    m_rootContainer->moveSurfacesToOutput(surfaces, singleOutput, nullptr);
    return true;
}

void OutputManager::restorePrimaryOutput()
{
    if (!m_config || !m_rootContainer) {
        return;
    }

    Output *primaryOutput = findOutputById(m_config->primaryOutputId());
    if (primaryOutput && primaryOutput->output() && primaryOutput->output()->isEnabled()) {
        m_rootContainer->setPrimaryOutput(primaryOutput);
        return;
    }

    if (auto *fallback = findFirstAvailableOutput(nullptr)) {
        m_rootContainer->setPrimaryOutput(fallback);
    }
}

OutputManager::CopyModeRestoreConfig OutputManager::copyModeRestoreConfig(int availableOutputCount) const
{
    CopyModeRestoreConfig result;
    if (!shouldRestoreCopyMode(availableOutputCount)) {
        return result;
    }
    result.outputIds = copyOutputIds();
    result.outputNames = outputNamesFromIds(result.outputIds);
    if (result.outputIds.size() < 2 || result.outputNames.size() < 2) {
        return {};
    }
    result.primaryOutput = findOutputById(result.outputIds.constFirst());
    result.name = m_config->copyOutputName();
    return result;
}

QStringList OutputManager::copyOutputIds() const
{
    return m_config ? deserializeOutputIds(m_config->copyOutputOutputs()) : QStringList{};
}

QStringList OutputManager::currentOutputIds(Output *primaryOutput) const
{
    QStringList ids;
    if (primaryOutput) {
        ids.append(outputId(primaryOutput));
    }
    if (!m_rootContainer) {
        return ids;
    }
    for (auto *output : std::as_const(m_rootContainer->outputs())) {
        if (output && output != primaryOutput) {
            ids.append(outputId(output));
        }
    }
    ids.removeAll(QString());
    return ids;
}

QStringList OutputManager::outputNamesFromIds(const QStringList &ids) const
{
    QStringList names;
    for (const auto &id : std::as_const(ids)) {
        if (auto *output = findOutputById(id)) {
            names.append(output->output()->name());
        }
    }
    return names;
}

void OutputManager::clearSingleOutputConfig()
{
    runWhenConfigInitialized([config = QPointer<TreelandConfig>(m_config)] {
        if (config) {
            config->setSingleOutputId(QString());
        }
    });
}

void OutputManager::storeSingleOutputConfig()
{
    QString singleOutputId;
    int enabledOutputCount = 0;
    if (m_rootContainer) {
        for (auto *output : std::as_const(m_rootContainer->outputs())) {
            if (!output || !output->output() || !output->output()->isEnabled()) {
                continue;
            }

            enabledOutputCount++;
            if (enabledOutputCount == 1) {
                singleOutputId = outputId(output);
            } else {
                singleOutputId.clear();
                break;
            }
        }
    }

    runWhenConfigInitialized([config = QPointer<TreelandConfig>(m_config), singleOutputId] {
        if (config) {
            config->setSingleOutputId(singleOutputId);
        }
    });
}

void OutputManager::storeCopyOutputConfig(bool enabled,
                                          const QString &name,
                                          const QStringList &outputIds)
{
    runWhenConfigInitialized([this, enabled, name, outputIds] {
        if (!m_config) {
            return;
        }
        if (!enabled) {
            const QString oldName = m_config->copyOutputName();
            m_config->setCreateCopyOutput(false);
            m_config->setCopyOutputName(QString());
            m_config->setCopyOutputOutputs(QStringLiteral("[]"));
            Q_EMIT copyOutputConfigurationChanged(false, oldName, {});
            return;
        }
        m_config->setSingleOutputId(QString());
        QString configName = name.isEmpty() ? m_config->copyOutputName() : name;
        if (configName.isEmpty()) {
            configName = QStringLiteral("copy-output");
        }
        m_config->setCreateCopyOutput(true);
        m_config->setCopyOutputName(configName);
        m_config->setCopyOutputOutputs(serializeOutputIds(outputIds));
        Q_EMIT copyOutputConfigurationChanged(true, configName, outputNamesFromIds(outputIds));
    });
}

bool OutputManager::takeCopyModeRestoreIntent()
{
    const bool result = m_copyModeRestoreIntent;
    m_copyModeRestoreIntent = false;
    return result;
}

void OutputManager::setCopyModeStored(bool enabled)
{
    if (!m_config) {
        return;
    }

    m_config->setCreateCopyOutput(enabled);
}

void OutputManager::clearCopyModeRestoreIntent()
{
    m_copyModeRestoreIntent = false;
}

Output *OutputManager::findFirstAvailableOutput(Output *excludeOutput) const
{
    if (!m_rootContainer) {
        return nullptr;
    }

    const auto &outputs = m_rootContainer->outputs();
    for (auto *output : std::as_const(outputs)) {
        if (output != excludeOutput && output && output->output() && output->output()->isEnabled()) {
            return output;
        }
    }

    return nullptr;
}

QString OutputManager::outputId(Output *output) const
{
    return output ? WallpaperManager::getOutputId(output) : QString();
}

Output *OutputManager::findOutputById(const QString &id) const
{
    if (!m_rootContainer) {
        return nullptr;
    }
    for (auto *output : std::as_const(m_rootContainer->outputs())) {
        if (outputId(output) == id) {
            return output;
        }
    }
    return nullptr;
}

void OutputManager::runWhenConfigInitialized(std::function<void()> callback)
{
    if (!m_config) {
        return;
    }
    if (m_config->isInitializeSucceeded()) {
        callback();
        return;
    }
    if (m_config->isInitializeFailed()) {
        return;
    }

    connect(m_config,
            &TreelandConfig::configInitializeSucceed,
            this,
            [callback = std::move(callback)] { callback(); });
}

void OutputManager::markScreenAsPrimaryIntent(Output *output)
{
    const QString id = outputId(output);
    if (!id.isEmpty()) {
        m_primaryRestoreIntents[id] = true;
    }
}

void OutputManager::restoreScreenAsPrimary(Output *output)
{
    if (!output || !m_rootContainer) {
        return;
    }

    m_rootContainer->setPrimaryOutput(output);
}

void OutputManager::switchPrimaryOutput(Output *from,
                                        Output *to,
                                        const QList<SurfaceWrapper *> &surfaces)
{
    if (!m_rootContainer || !to) {
        return;
    }

    m_rootContainer->setPrimaryOutput(to);
    m_rootContainer->moveSurfacesToOutput(surfaces, to, from);
}

void OutputManager::onScreenAdded(Output *output, const QList<SurfaceWrapper *> &surfaces)
{
    if (!output || !m_rootContainer) {
        return;
    }

    const QString id = outputId(output);
    const bool wasPrimary = m_primaryRestoreIntents.value(id);
    const bool hasPrimaryOutput = m_rootContainer->primaryOutput() != nullptr;

    if (m_config && id == m_config->singleOutputId()) {
        restoreConfiguredSingleOutput(surfaces);
        m_primaryRestoreIntents.remove(id);
        return;
    }

    if (wasPrimary && m_mode == Mode::Extension && hasPrimaryOutput) {
        restoreScreenAsPrimary(output);
    }

    m_primaryRestoreIntents.remove(id);
}

bool OutputManager::onScreenRemoved(Output *output,
                                    const QList<SurfaceWrapper *> &surfaces)
{
    if (!output || !m_rootContainer) {
        return false;
    }

    const bool isCurrentPrimary = (m_rootContainer->primaryOutput() == output);
    const bool wasPrimaryBeforeRemoval = m_primaryRestoreIntents.value(outputId(output));

    if (m_config && outputId(output) == m_config->singleOutputId()) {
        return true;
    }

    if (isCurrentPrimary && !wasPrimaryBeforeRemoval) {
        markScreenAsPrimaryIntent(output);
    }

    if (!isCurrentPrimary) {
        if (auto *primaryOutput = m_rootContainer->primaryOutput()) {
            m_rootContainer->moveSurfacesToOutput(surfaces, primaryOutput, output);
        }
    }
    return false;
}

void OutputManager::onScreenDisabled(Output *output,
                                     const QList<SurfaceWrapper *> &surfaces)
{
    if (!output || !m_rootContainer) {
        return;
    }

    const bool isCurrentPrimary = (m_rootContainer->primaryOutput() == output);

    if (m_mode == Mode::Copy && isCurrentPrimary) {
        setCopyModeStored(true);
    } else if (isCurrentPrimary) {
        markScreenAsPrimaryIntent(output);
    }

    if (isCurrentPrimary && !m_rootContainer->outputs().isEmpty()) {
        if (auto *nextPrimary = findFirstAvailableOutput(output)) {
            switchPrimaryOutput(output, nextPrimary, surfaces);
        }
    } else if (!isCurrentPrimary) {
        if (auto *primaryOutput = m_rootContainer->primaryOutput()) {
            m_rootContainer->moveSurfacesToOutput(surfaces, primaryOutput, output);
        }
    }
}

void OutputManager::onScreenEnabled(Output *output)
{
    if (!output || !m_rootContainer) {
        return;
    }

    const QString id = outputId(output);
    const bool wasPrimary = m_primaryRestoreIntents.value(id);
    if (wasPrimary && m_mode == Mode::Extension && m_rootContainer->primaryOutput()) {
        restoreScreenAsPrimary(output);
    }

    m_primaryRestoreIntents.remove(id);
}
