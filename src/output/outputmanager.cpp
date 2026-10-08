// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscopedvalue.h>
#include "outputmanager.h"

#include "common/treelandlogging.h"
#include "core/dconfigmanager.h"
#include "core/rootsurfacecontainer.h"
#include "modules/virtual-output/virtualoutputmanagerinterfacev1.h"
#include "output.h"
#include "outputconfig.hpp"
#include "session/session.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "wallpaper/wallpapermanager.h"
#include "workspace/workspace.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QCursor>
#include <QPointer>
#include <QQmlEngine>
#include <QScopeGuard>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <WBackend>
#include <WXWayland>
#include <wlr_all.h>
#include <woutputhelper.h>
#include <woutputlayout.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wxdgoutput.h>

#include <rhi/qrhi.h>

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

bool hasSavedOutputState(OutputConfig *config)
{
    return config && (!config->widthIsDefaultValue()
                      || !config->heightIsDefaultValue()
                      || !config->refreshIsDefaultValue()
                      || !config->scaleIsDefaultValue()
                      || !config->transformIsDefaultValue()
                      || !config->adaptiveSyncEnabledIsDefaultValue());
}

bool outputConfigInitializationFinished(OutputConfig *config)
{
    return config && (config->isInitializeSucceeded() || config->isInitializeFailed());
}

bool outputMatchesId(Output *output, const QString &outputId)
{
    return output && output->output() && output->output()->isEnabled()
        && output->getOutputId() == outputId;
}

bool currentPrimaryMatchesId(RootSurfaceContainer *rootContainer, const QString &outputId)
{
    return rootContainer && outputMatchesId(rootContainer->primaryOutput(), outputId);
}

wlr_output_mode *closestOutputMode(WAYLIB_SERVER_NAMESPACE::WOutput *output,
                                   int width,
                                   int height,
                                   int refresh)
{
    if (!output || !output->handle()) {
        return nullptr;
    }

    wlr_output_mode *mode = nullptr;
    wlr_output_mode *closestMode = nullptr;
    qint64 closestResolutionDistance = std::numeric_limits<qint64>::max();
    qint64 closestRefreshDistance = std::numeric_limits<qint64>::max();
    wl_list_for_each(mode, &output->handle()->modes, link) {
        const qint64 resolutionDistance = std::abs(static_cast<qint64>(mode->width) - width)
            + std::abs(static_cast<qint64>(mode->height) - height);
        const qint64 refreshDistance = std::abs(static_cast<qint64>(mode->refresh) - refresh);
        if (resolutionDistance < closestResolutionDistance
            || (resolutionDistance == closestResolutionDistance
                && refreshDistance < closestRefreshDistance)) {
            closestMode = mode;
            closestResolutionDistance = resolutionDistance;
            closestRefreshDistance = refreshDistance;
        }
        if (resolutionDistance == 0 && refreshDistance == 0) {
            break;
        }
    }
    return closestMode;
}
}

OutputManager::OutputManager(RootSurfaceContainer *rootContainer,
                             TreelandConfig *config,
                             WallpaperManager *wallpaperManager,
                             Workspace *workspace,
                             WOutputRenderWindow *renderWindow,
                             QObject *parent)
    : QObject(parent)
    , m_rootContainer(rootContainer)
    , m_config(config)
    , m_wallpaperManager(wallpaperManager)
    , m_workspace(workspace)
    , m_renderWindow(renderWindow)
{
    Q_ASSERT(m_rootContainer);
    Q_ASSERT(m_config);
    Q_ASSERT(m_wallpaperManager);
    Q_ASSERT(m_workspace);
    Q_ASSERT(m_renderWindow);
}

void OutputManager::setBackend(WBackend *backend)
{
    Q_ASSERT(!m_backend);
    m_backend = backend;
    connect(m_backend, &WBackend::outputAdded, this, &OutputManager::outputAdded);
    connect(m_backend, &WBackend::outputRemoved, this, &OutputManager::outputRemoved);
}

void OutputManager::setOutputManagementProtocol(WOutputManagerV1 *manager)
{
    Q_ASSERT(!m_outputManagementProtocol);
    m_outputManagementProtocol = manager;
    connect(m_outputManagementProtocol,
            &WOutputManagerV1::requestTestOrApply,
            this,
            &OutputManager::testOrApplyConfiguration);
}

void OutputManager::setVirtualOutputInterface(VirtualOutputManagerInterfaceV1 *interface)
{
    Q_ASSERT(!m_virtualOutputInterface);
    m_virtualOutputInterface = interface;
    connect(m_virtualOutputInterface,
            &VirtualOutputManagerInterfaceV1::requestCreateVirtualOutput,
            this,
            &OutputManager::createVirtualOutput);
    connect(m_virtualOutputInterface,
            &VirtualOutputManagerInterfaceV1::destroyVirtualOutput,
            this,
            &OutputManager::destroyVirtualOutput);
}

void OutputManager::setGammaControlManager(wlr_gamma_control_manager_v1 *manager)
{
    m_outputListenerOwner.listeners()->add(
        &manager->events.set_gamma, this, &OutputManager::setGamma);
}

void OutputManager::setOutputPowerManager(wlr_output_power_manager_v1 *manager)
{
    m_outputListenerOwner.listeners()->add(
        &manager->events.set_mode, this, &OutputManager::setPowerMode);
}

void OutputManager::setXdgOutputManagers(WXdgOutputManager *manager,
                                         WXdgOutputManager *xwaylandManager,
                                         SessionManager *sessionManager)
{
    Q_ASSERT(!m_xwaylandOutputManager);

    m_xwaylandOutputManager = xwaylandManager;
    m_xwaylandOutputManager->setScaleOverride(1.0);

    const auto isXWaylandClient = [sessionManager = QPointer(sessionManager)](WClient *client) {
        if (!sessionManager)
            return false;

        for (const auto &session : std::as_const(sessionManager->sessions())) {
            if (session && session->xwayland()
                && session->xwayland()->waylandClient() == client) {
                return true;
            }
        }
        return false;
    };
    manager->setFilter([isXWaylandClient](WClient *client) {
        return !isXWaylandClient(client);
    });
    m_xwaylandOutputManager->setFilter(isXWaylandClient);

    // XWayland perceives screen geometry in physical-pixel space scaled by
    // maxDPR so root-window and xrandr geometry match client coordinates.
    auto updateXWaylandOutputScale = [this] {
        m_xwaylandOutputManager->setScaleOverride(
            m_renderWindow->effectiveDevicePixelRatio());
    };
    connect(m_renderWindow,
            &WOutputRenderWindow::effectiveDevicePixelRatioChanged,
            this,
            updateXWaylandOutputScale);
    updateXWaylandOutputScale();
}

WXdgOutputManager *OutputManager::xwaylandOutputManager() const
{
    return m_xwaylandOutputManager;
}

bool OutputManager::isNvidiaCardPresent() const
{
    auto *rhi = m_renderWindow->rhi();
    if (!rhi)
        return false;

    const QString deviceName = rhi->driverInfo().deviceName;
    qCDebug(lcTlOutput) << "Graphics Device:" << deviceName;
    return deviceName.contains("NVIDIA", Qt::CaseInsensitive);
}

void OutputManager::processOutputAdded(WOutput *output)
{
    allowNonDrmOutputAutoChangeMode(output);
    Output *o = nullptr;
    const bool isInitialOutput = !m_initialOutputScanFinished;
    qCInfo(lcTlOutput) << "Output added" << output->name()
                       << "id:" << Output::getOutputId(output->handle())
                       << "scan complete:" << m_initialOutputScanFinished
                       << "mode:" << static_cast<int>(m_mode);

    if (!m_initialOutputScanFinished) {
        // The initial scan collects normal outputs first. Copy mode is restored once,
        // after backend start has reported all outputs.
        o = createNormalOutput(output);
    } else if (m_mode == Mode::Extension || !m_rootContainer->primaryOutput()) {
        o = createNormalOutput(output);
    } else if (m_mode == Mode::Copy) {
        o = createCopyOutput(output, m_rootContainer->primaryOutput());
    }
    m_outputs.append(o);
    const bool outputRegistered = ensureInRootContainer(o);
    if (!outputRegistered) {
        qCWarning(lcTlOutput) << "Failed to register output in root container" << output->name();
    }
    if (m_initialOutputScanFinished && m_mode == Mode::Copy
        && outputRegistered && !output->isEnabled()) {
        o->enable();
    }
    if (m_initialOutputScanFinished) {
        onScreenAdded(o, workspaceSurfaces());
    }

    if (m_initialOutputScanFinished && m_mode == Mode::Copy) {
        QStringList copyOutputs = copyOutputIds();
        const QString addedOutputId = o->getOutputId();
        if (!copyOutputs.contains(addedOutputId)) {
            copyOutputs.append(addedOutputId);
            storeCopyOutputConfig(true, {}, copyOutputs);
        }
    }
    if (m_initialOutputScanFinished && m_mode == Mode::Extension) {
        const QString addedOutputId = o->getOutputId();
        QMetaObject::invokeMethod(this, [this, addedOutputId] {
            if (m_mode != Mode::Extension || !m_config->createCopyOutput()) {
                return;
            }

            QStringList configuredCopyOutputs = copyOutputIds();
            if (!configuredCopyOutputs.contains(addedOutputId)) {
                const bool waitingForAnotherCopyMember = configuredCopyOutputs.size() == 1
                    && findOutputById(configuredCopyOutputs.constFirst());
                if (!waitingForAnotherCopyMember) {
                    storeCopyOutputConfig(false);
                    return;
                }

                configuredCopyOutputs.append(addedOutputId);
                storeCopyOutputConfig(true, {}, configuredCopyOutputs);
            }

            const bool allCopyOutputsAvailable =
                configuredCopyOutputs.size() >= 2
                && std::all_of(configuredCopyOutputs.cbegin(),
                               configuredCopyOutputs.cend(),
                               [this](const QString &id) { return findOutputById(id); });
            if (allCopyOutputsAvailable) {
                restoreConfiguredCopyMode();
            }
        }, Qt::QueuedConnection);
    }
    // The output-management protocol must advertise an output as soon as it
    // enters the compositor. DConfig restoration is asynchronous and may be
    // unavailable in minimal sessions; delaying registration until it
    // completes leaves newly bound clients with an empty head list forever.
    m_outputManagementProtocol->newOutput(output);

    const bool shouldDisableOutput = !m_initialOutputScanFinished;
    if (shouldDisableOutput) {
        WOutputStateGuard disabledState;
        wlr_output_state_set_enabled(disabledState.get(), false);
        if (!wlr_output_commit_state(output->handle(), disabledState.get())) {
            qCCritical(lcTlCore) << "commit failed while disabling added output" << output->name();
        } else if (!m_initialOutputScanFinished) {
            qCInfo(lcTlOutput) << "Temporarily disabled output during initial scan" << output->name();
        }
    }

    auto publishOutput = [this, outputObject = QPointer<Output>(o)] {
        if (!outputObject) {
            return;
        }

        m_wallpaperManager->ensureWallpaperConfigForOutput(outputObject);
    };
    auto restoreOutputConfig = [this,
                                output,
                                outputObject = QPointer<Output>(o),
                                publishOutput,
                                isInitialOutput] {
        auto publish = qScopeGuard(publishOutput);
        if (!outputObject || m_mode == Mode::Copy) {
            return;
        }

        // Only the initial backend scan restores saved geometry. A hot-plugged
        // output keeps outputLayout's auto-added position, while its mode,
        // transform, scale, brightness, and color temperature are restored.
        if (!isInitialOutput) {
            const bool restoreAsExtensionOutput =
                m_mode == Mode::Extension
                && m_config->singleOutputId().isEmpty()
                && !m_config->createCopyOutput();
            if (restoreAsExtensionOutput && !output->isEnabled()) {
                outputObject->enable();
            }
        }

        const QString singleOutputId = m_config->singleOutputId();
        if (!singleOutputId.isEmpty()
            && outputObject->getOutputId() != singleOutputId) {
            if (output->isEnabled()) {
                WOutputStateGuard disabledState;
                wlr_output_state_set_enabled(disabledState.get(), false);
                if (!wlr_output_commit_state(output->handle(), disabledState.get())) {
                    qCCritical(lcTlOutput)
                        << "Failed to disable non-selected output while restoring single-output display"
                        << output->name();
                    return;
                }
            }
            if (auto *layout = m_rootContainer->outputLayout();
                layout && layout->outputs().contains(output)) {
                layout->remove(output);
            }
            qCInfo(lcTlOutput) << "Disabled non-selected output while restoring single-output display"
                               << output->name()
                               << "selected output id:" << singleOutputId;
            return;
        }

        auto restoreColorConfig = qScopeGuard([outputObject] {
            if (outputObject && outputObject->output() && outputObject->output()->isEnabled()) {
                outputObject->applyOutputColorConfig();
            }
        });

        auto *config = outputObject->config();
        const QString outputId = outputObject->getOutputId();
        const QString primaryOutputId = m_config->primaryOutputId();
        if (primaryOutputId == outputId) {
            m_rootContainer->setPrimaryOutput(outputObject);
        } else if (m_rootContainer->primaryOutput()
                   && m_rootContainer->primaryOutput()->output()
                   && !m_rootContainer->primaryOutput()->output()->isEnabled()
                   && !currentPrimaryMatchesId(m_rootContainer, primaryOutputId)) {
            m_rootContainer->setPrimaryOutput(outputObject);
        }

        if (!hasSavedOutputState(config)) {
            if (!output->isEnabled()) {
                outputObject->enable();
            }
            return;
        }

        const int width = static_cast<int>(config->width());
        const int height = static_cast<int>(config->height());
        const int refresh = static_cast<int>(config->refresh());
        const double scale = config->scale();
        const qlonglong transform = config->transform();
        if (width <= 0 || height <= 0 || refresh <= 0 || scale <= 0.0) {
            qCWarning(lcTlCore) << "Ignoring invalid output dconfig for" << output->name()
                                << "width:" << width
                                << "height:" << height
                                << "refresh:" << refresh
                                << "scale:" << scale;
            return;
        }
        if (transform < WL_OUTPUT_TRANSFORM_NORMAL || transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
            qCWarning(lcTlCore) << "Ignoring invalid output dconfig for" << output->name()
                                << "transform:" << transform;
            return;
        }

        WOutputStateGuard newState;
        wlr_output_state_set_enabled(newState.get(), true);

        if (auto *layout = m_rootContainer->outputLayout()) {
            layout->move(output, QPoint(static_cast<int>(config->x()), static_cast<int>(config->y())));
        }

        if (auto *mode = closestOutputMode(output, width, height, refresh)) {
            wlr_output_state_set_mode(newState.get(), mode);
        } else {
            wlr_output_state_set_custom_mode(newState.get(), width, height, refresh);
        }

        wlr_output_state_set_adaptive_sync_enabled(newState.get(), config->adaptiveSyncEnabled());
        wlr_output_state_set_transform(newState.get(), static_cast<wl_output_transform>(transform));
        wlr_output_state_set_scale(newState.get(), scale);
        const bool commitOk = wlr_output_commit_state(output->handle(), newState.get());
        if (!commitOk) {
            qCCritical(lcTlCore) << "commit failed on output" << output->name();
            return;
        }

        if (auto *outputItem = outputObject->outputItem()) {
            QMetaObject::invokeMethod(outputItem,
                                      "setTransform",
                                      Q_ARG(QVariant, QVariant::fromValue(static_cast<WOutput::Transform>(transform))));
        }

        saveCurrentConfig(outputObject);
    };
    auto *outputConfig = o->config();
    if (outputConfig->isInitializeSucceeded()) {
        restoreOutputConfig();
    } else {
        publishOutput();
    }
}

void OutputManager::outputAdded(WOutput *output)
{
    auto *configManager = DConfigManager::instance();

    auto *config = configManager->outputConfig(Output::getOutputId(output->handle()));
    if (outputConfigInitializationFinished(config)) {
        processOutputAdded(output);
        finishInitialOutputScanIfReady();
        return;
    }

    if (m_pendingOutputs.contains(output)) {
        return;
    }

    m_pendingOutputs.insert(output);
    connect(output, &QObject::destroyed, this, [this, output] {
        if (m_pendingOutputs.remove(output)) {
            finishInitialOutputScanIfReady();
        }
    });

    auto continueOutputAdded = [this, output] {
        if (m_pendingOutputs.remove(output)) {
            processOutputAdded(output);
            finishInitialOutputScanIfReady();
        }
    };
    connect(config,
            &OutputConfig::configInitializeSucceed,
            output,
            continueOutputAdded,
            Qt::SingleShotConnection);
    connect(config,
            &OutputConfig::configInitializeFailed,
            output,
            continueOutputAdded,
            Qt::SingleShotConnection);
}

void OutputManager::finishInitialOutputScanIfReady()
{
    if (m_initialOutputScanFinished || !m_backendStartFinished || !m_pendingOutputs.isEmpty()) {
        return;
    }

    m_initialOutputScanFinished = true;
    restoreInitialOutputConfiguration();
}

void OutputManager::backendStarted()
{
    m_backendStartFinished = true;
    finishInitialOutputScanIfReady();
}

void OutputManager::wakePoweredOffOutputs()
{
    for (auto *output : std::as_const(m_outputs)) {
        auto *wlrOutput = output->output()->handle();
        if (wlrOutput->enabled || !wlrOutput->current_mode
            || !m_powerOffOutputs.contains(wlrOutput)) {
            continue;
        }

        WOutputStateGuard state;
        wlr_output_state_set_enabled(state.get(), true);
        if (!wlr_output_commit_state(wlrOutput, state.get())) {
            qCWarning(lcTlCore) << "Failed to wake output" << wlrOutput->name;
            continue;
        }
        m_powerOffOutputs.remove(wlrOutput);
    }
}

void OutputManager::outputRemoved(WOutput *output)
{
    if (m_pendingOutputs.remove(output)) {
        finishInitialOutputScanIfReady();
        return;
    }

    output->removeListeners(&m_outputListenerOwner);
    auto index = indexOf(output);
    Q_ASSERT(index >= 0);
    const auto o = m_outputs.takeAt(index);

    const auto &surfaces = workspaceSurfaces(o);
    const QStringList copyOutputs = copyOutputIds();
    const bool removedCopyOutput = copyOutputs.contains(o->getOutputId());
    if (m_mode == Mode::Copy && removedCopyOutput) {
        const bool removedCopySource = !copyOutputs.isEmpty()
            && copyOutputs.constFirst() == o->getOutputId();

        if (removedCopySource && !m_outputs.isEmpty()) {
            Output *newCopySource = nullptr;
            for (const auto &outputId : std::as_const(copyOutputs)) {
                newCopySource = findOutputById(outputId);
                if (newCopySource) {
                    break;
                }
            }
            if (!newCopySource) {
                newCopySource = m_outputs.constFirst();
            }

            const auto newCopySourceId = newCopySource->getOutputId();
            QStringList updatedCopyOutputs{ newCopySourceId };
            for (const auto &outputId : std::as_const(copyOutputs)) {
                if (outputId != newCopySourceId && findOutputById(outputId)) {
                    updatedCopyOutputs.append(outputId);
                }
            }

            const int newCopySourceIndex = m_outputs.indexOf(newCopySource);
            removeFromRootContainer(newCopySource);
            Output *normalCopySource = createNormalOutput(newCopySource->output());
            normalCopySource->enable();
            m_outputs.replace(newCopySourceIndex, normalCopySource);
            newCopySource->deleteLater();

            for (int i = 0; i < m_outputs.size(); ++i) {
                Output *copyOutput = m_outputs.at(i);
                if (copyOutput == normalCopySource
                    || !copyOutputs.contains(copyOutput->getOutputId())) {
                    continue;
                }

                removeFromRootContainer(copyOutput);
                Output *replacement = createCopyOutput(copyOutput->output(), normalCopySource);
                replacement->enable();
                m_rootContainer->addOutput(replacement);
                m_outputs.replace(i, replacement);
                copyOutput->deleteLater();
            }

            m_rootContainer->setPrimaryOutput(normalCopySource);
            if (!surfaces.isEmpty()) {
                moveSurfaces(surfaces, normalCopySource, o);
            }
            removeFromRootContainer(o);

            // Persist only the active copy group. A subsequently connected
            // output is added as a new member, regardless of whether it is the
            // disconnected source or a different output.
            storeCopyOutputConfig(true, {}, updatedCopyOutputs);
        } else {

            m_mode = Mode::Extension;
            Q_EMIT modeChanged();

            QList<Output *> outputsToConvert;
            QList<Output *> oldOutputsToDelete;

            bool removedWasPrimary = (output == m_rootContainer->primaryOutput()->output());
            Output *sourceCandidate = nullptr;

            for (int i = 0; i < m_outputs.size(); i++) {
                Output *copyOutput = m_outputs.at(i);

                if (copyOutput->isSource()) {
                    if (!sourceCandidate)
                        sourceCandidate = copyOutput;
                    continue;
                }

                removeFromRootContainer(copyOutput);
                Output *normalOutput = createNormalOutput(copyOutput->output());
                normalOutput->enable();
                saveCurrentConfig(normalOutput);

                outputsToConvert.append(normalOutput);
                oldOutputsToDelete.append(copyOutput);

                m_outputs.replace(i, normalOutput);

                if (!sourceCandidate) {
                    sourceCandidate = normalOutput;
                }
            }

            if (removedWasPrimary && sourceCandidate) {
                m_rootContainer->setPrimaryOutput(sourceCandidate);
                if (!surfaces.isEmpty()) {
                    moveSurfaces(surfaces, sourceCandidate, o);
                }
            }

            removeFromRootContainer(o);

            for (auto oldOutput : std::as_const(oldOutputsToDelete)) {
                delete oldOutput;
            }
        }

    } else {
        removeFromRootContainer(o);
        const bool removedConfiguredSingleOutput = onScreenRemoved(o, surfaces);
        if (removedConfiguredSingleOutput) {
            // Keep the unavailable output as the configured single-output target.
            // The remaining outputs are enabled only as a temporary fallback.
            restoreExtensionModeFromConfig(true);
        }
    }

    m_outputManagementProtocol->removeOutput(output);
    m_wallpaperManager->removeOutputWallpaper(output->handle());

    m_powerOffOutputs.remove(output->handle());

    delete o;
}

OutputManager::Mode OutputManager::mode() const
{
    return m_mode;
}

const QList<Output *> &OutputManager::outputs() const
{
    return m_outputs;
}

Output *OutputManager::createNormalOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    Output *result = Output::create(output, ::qmlEngine(m_renderWindow), parent());
    if (isNvidiaCardPresent()) {
        result->outputItem()->setProperty("forceSoftwareCursor", true);
    }
    result->outputItem()->stackBefore(m_rootContainer);
    removeFromRootContainer(output);
    m_rootContainer->addOutput(result);
    return result;
}

Output *OutputManager::createCopyOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output,
                                        Output *proxy)
{
    return Output::createCopy(output, proxy, ::qmlEngine(m_renderWindow), parent());
}

bool OutputManager::ensureInRootContainer(Output *output)
{
    if (!output || !output->output()) {
        return false;
    }

    auto *layout = m_rootContainer->outputLayout();
    if (!layout) {
        return false;
    }

    const bool inRoot = m_rootContainer->outputs().contains(output);
    const bool inLayout = layout->outputs().contains(output->output());
    if (inRoot && inLayout) {
        return true;
    }

    qCInfo(lcTlOutput) << "Re-registering output before applying output configuration"
                       << output->output()->name()
                       << "in root:" << inRoot
                       << "in layout:" << inLayout;

    if (!inRoot && inLayout) {
        removeFromRootContainer(output->output());
    }

    if (!inRoot) {
        m_rootContainer->addOutput(output);
    } else if (!inLayout) {
        layout->autoAdd(output->output());
    }

    return m_rootContainer->outputs().contains(output)
        && layout->outputs().contains(output->output());
}

void OutputManager::removeFromRootContainer(Output *output)
{
    if (!output || !output->output()) {
        return;
    }

    auto *layout = m_rootContainer->outputLayout();
    const bool inRoot = m_rootContainer->outputs().contains(output);
    const bool inLayout = layout && layout->outputs().contains(output->output());
    if (!inRoot && !inLayout) {
        return;
    }

    if (!inRoot || !inLayout) {
        qCWarning(lcTlCore) << "Output root/layout registration is inconsistent before removal"
                            << output->output()->name()
                            << "in root:" << inRoot
                            << "in layout:" << inLayout;
        if (inRoot && !inLayout) {
            m_rootContainer->outputModel()->removeObject(output);
            m_rootContainer->SurfaceContainer::removeOutput(output);
        } else if (!inRoot && inLayout) {
            layout->remove(output->output());
        }
        return;
    }

    m_rootContainer->removeOutput(output);
}

void OutputManager::removeFromRootContainer(WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    if (!output) {
        return;
    }

    for (auto *rootOutput : std::as_const(m_rootContainer->outputs())) {
        if (rootOutput && rootOutput->output() == output) {
            removeFromRootContainer(rootOutput);
            return;
        }
    }

    auto *layout = m_rootContainer->outputLayout();
    if (layout && layout->outputs().contains(output)) {
        qCWarning(lcTlCore) << "Removing stale output layout entry before re-registering"
                            << output->name();
        layout->remove(output);
    }
}

WAYLIB_SERVER_NAMESPACE::WOutputViewport *OutputManager::ownOutputViewport(
    WAYLIB_SERVER_NAMESPACE::WOutput *output) const
{
    Output *outputObject = outputFor(output);
    if (!outputObject || !outputObject->outputItem()) {
        qCWarning(lcTlCore) << "Invalid output object for" << output->name();
        return nullptr;
    }

    auto *viewport = outputObject->outputItem()->findChild<
        WAYLIB_SERVER_NAMESPACE::WOutputViewport *>({}, Qt::FindDirectChildrenOnly);
    if (!viewport) {
        qCWarning(lcTlCore) << "No viewport found for output" << output->name()
                            << "- OutputItem may not have been fully initialized";
    }
    return viewport;
}

QList<SurfaceWrapper *> OutputManager::workspaceSurfaces(Output *filterOutput) const
{
    QList<SurfaceWrapper *> surfaces;
    WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow::paintOrderItemList(
        m_workspace,
        [this, &surfaces, filterOutput](QQuickItem *item) -> bool {
            auto *surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper
                && surfaceWrapper->showOnWorkspace(m_workspace->current()->id())
                && (!filterOutput || surfaceWrapper->ownsOutput() == filterOutput)) {
                surfaces.append(surfaceWrapper);
                return true;
            }
            return false;
        });
    return surfaces;
}

void OutputManager::moveSurfaces(const QList<SurfaceWrapper *> &surfaces,
                                 Output *targetOutput,
                                 Output *sourceOutput)
{
    m_rootContainer->moveSurfacesToOutput(surfaces, targetOutput, sourceOutput);
}

void OutputManager::saveCurrentConfig(Output *output)
{
    if (!output) {
        return;
    }

    auto *outputConfig = output->config();
    if (!outputConfig || !outputConfig->isInitializeSucceeded()) {
        return;
    }
    if (!output->output() || !output->output()->handle()->current_mode) {
        return;
    }

    auto *wlrOutput = output->output()->handle();
    auto *currentMode = wlrOutput->current_mode;
    outputConfig->setWidth(currentMode->width);
    outputConfig->setHeight(currentMode->height);
    outputConfig->setRefresh(currentMode->refresh);
    outputConfig->setTransform(wlrOutput->transform);
    outputConfig->setScale(wlrOutput->scale);
    outputConfig->setAdaptiveSyncEnabled(
        wlrOutput->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED);

    if (auto *layout = output->output()->layout()) {
        if (auto *layoutOutput = wlr_output_layout_get(layout->handle(), wlrOutput)) {
            outputConfig->setX(layoutOutput->x);
            outputConfig->setY(layoutOutput->y);
        }
    }
}

int OutputManager::indexOf(WAYLIB_SERVER_NAMESPACE::WOutput *output) const
{
    for (int i = 0; i < m_outputs.size(); ++i) {
        if (m_outputs.at(i)->output() == output) {
            return i;
        }
    }
    return -1;
}

Output *OutputManager::outputFor(WAYLIB_SERVER_NAMESPACE::WOutput *output) const
{
    for (auto *candidate : std::as_const(m_outputs)) {
        if (candidate->output() == output) {
            return candidate;
        }
    }
    return nullptr;
}

Output *OutputManager::findOutputByName(const QString &name) const
{
    for (auto *output : std::as_const(m_outputs)) {
        if (output && output->output() && output->output()->name() == name) {
            return output;
        }
    }
    return nullptr;
}

Output *OutputManager::findOutputById(const QString &id) const
{
    for (auto *output : std::as_const(m_outputs)) {
        if (output && output->output() && output->getOutputId() == id) {
            return output;
        }
    }
    return nullptr;
}

void OutputManager::requestAdditionalOutputs()
{
    if (wlr_backend_is_multi(m_backend->handle())) {
        wlr_multi_for_each_backend(
            m_backend->handle(),
            [](wlr_backend *backend, void *) {
                if (wlr_backend_is_x11(backend)) {
                    wlr_x11_output_create(backend);
                } else if (wlr_backend_is_wl(backend)) {
                    wlr_wl_output_create(backend);
                }
            },
            nullptr);
    }
}

void OutputManager::applyMode(Mode mode)
{
    if (m_outputs.isEmpty()) {
        return;
    }

    const bool refreshExtensionMode = m_mode == mode && mode == Mode::Extension;
    if (m_mode == mode && !refreshExtensionMode) {
        return;
    }

    if (refreshExtensionMode) {
        clearSingleOutputConfig();
        storeCopyOutputConfig(false);
        restoreExtensionModeFromConfig();
        return;
    }

    m_mode = mode;
    if (mode == Mode::Extension) {
        clearSingleOutputConfig();
        restoreExtensionModeFromConfig();
    }
    storeCopyOutputConfig(
        mode == Mode::Copy,
        QStringLiteral("copy-output"),
        currentOutputIds(m_rootContainer->primaryOutput()));
    Q_EMIT modeChanged();

    for (int i = 0; i < m_outputs.size(); ++i) {
        if (m_outputs.at(i) == m_rootContainer->primaryOutput()) {
            continue;
        }

        Output *replacement = nullptr;
        if (mode == Mode::Copy) {
            removeFromRootContainer(m_outputs.at(i));
            replacement = createCopyOutput(m_outputs.at(i)->output(),
                                           m_rootContainer->primaryOutput());
            m_rootContainer->addOutput(replacement);
        } else {
            removeFromRootContainer(m_outputs.at(i));
            replacement = createNormalOutput(m_outputs.at(i)->output());
            replacement->enable();
            saveCurrentConfig(replacement);
        }
        m_outputs.at(i)->deleteLater();
        m_outputs.replace(i, replacement);
    }
}

void OutputManager::allowNonDrmOutputAutoChangeMode(
    WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    output->listeners(&m_outputListenerOwner)->add(
        &output->handle()->events.request_state,
        this,
        [output](wlr_output_event_request_state *newState) {
            if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
                if (!wlr_output_commit_state(output->handle(), newState->state)) {
                    qCCritical(lcTlCore, "commit failed on output %s", output->handle()->name);
                }
            }
        });
}

Output *OutputManager::outputAtCursor() const
{
    const QPoint cursorPosition = QCursor::pos();
    for (auto *output : std::as_const(m_outputs)) {
        const QRectF geometry(output->outputItem()->position(), output->outputItem()->size());
        if (geometry.contains(cursorPosition)) {
            return output;
        }
    }
    return m_rootContainer->primaryOutput();
}

void OutputManager::enableAllOutputs()
{
    for (auto *output : std::as_const(m_outputs)) {
        if (!output || !output->output()) {
            continue;
        }

        WOutputStateGuard state;
        wlr_output_state_set_enabled(state.get(), true);
        const bool ok = wlr_output_commit_state(output->output()->handle(), state.get());
        if (!ok) {
            qCWarning(lcTlOutput) << "Failed to enable output" << output->output()->name();
            continue;
        }

        if (auto *layout = m_rootContainer->outputLayout();
            layout && !layout->outputs().contains(output->output())) {
            layout->autoAdd(output->output());
        }
    }
}

void OutputManager::applyCopyModeToOutputs(Output *primaryOutput,
                                           const QList<SurfaceWrapper *> &surfaces,
                                           const QStringList &outputIds,
                                           bool persistConfig)
{
    if (primaryOutput->output() && !primaryOutput->output()->isEnabled()) {
        primaryOutput->enable();
    }
    if (auto *layout = m_rootContainer->outputLayout();
        primaryOutput->output()
        && primaryOutput->output()->isEnabled()
        && layout
        && !layout->outputs().contains(primaryOutput->output())) {
        layout->autoAdd(primaryOutput->output());
    }

    for (int i = 0; i < m_outputs.size(); ++i) {
        Output *existingOutput = m_outputs.at(i);
        if (existingOutput == primaryOutput) {
            continue;
        }
        if (!outputIds.isEmpty() && !outputIds.contains(existingOutput->getOutputId())) {
            continue;
        }

        removeFromRootContainer(existingOutput);
        Output *copyOutput = createCopyOutput(existingOutput->output(), primaryOutput);
        existingOutput->deleteLater();
        m_outputs.replace(i, copyOutput);
        m_rootContainer->addOutput(copyOutput);
        copyOutput->enable();
    }

    m_mode = Mode::Copy;
    clearCopyModeRestoreIntent();
    if (persistConfig) {
        storeCopyOutputConfig(
            true,
            {},
            outputIds.isEmpty() ? currentOutputIds(primaryOutput) : outputIds);
    }
    Q_EMIT modeChanged();

    if (!surfaces.isEmpty()) {
        moveSurfaces(surfaces, primaryOutput, nullptr);
    }
}

bool OutputManager::restoreConfiguredCopyMode()
{
    if (m_mode != Mode::Extension) {
        return false;
    }

    const auto restoreConfig = copyModeRestoreConfig(m_outputs.size());
    if (!restoreConfig) {
        return false;
    }

    qCInfo(lcTlOutput) << "Restoring configured Copy Mode"
                       << "name:" << restoreConfig.name
                       << "ids:" << restoreConfig.outputIds
                       << "outputs:" << restoreConfig.outputNames;
    if (m_virtualOutputInterface && !restoreConfig.name.isEmpty()) {
        m_virtualOutputInterface->restoreVirtualOutput(
            restoreConfig.name, restoreConfig.outputNames);
    }

    m_rootContainer->setPrimaryOutput(restoreConfig.primaryOutput);
    const auto surfaces = workspaceSurfaces();
    applyCopyModeToOutputs(
        restoreConfig.primaryOutput, surfaces, restoreConfig.outputIds, false);
    return true;
}

void OutputManager::restoreExtensionModeFromConfig(bool preserveSingleOutputConfig)
{
    if (m_outputs.isEmpty()) {
        return;
    }

    m_mode = Mode::Extension;
    Q_EMIT modeChanged();

    for (auto *outputObject : std::as_const(m_outputs)) {
        if (!outputObject || !outputObject->output()) {
            continue;
        }

        outputObject->enable();
        if (auto *layout = m_rootContainer->outputLayout();
            outputObject->output()->isEnabled()
            && layout
            && !layout->outputs().contains(outputObject->output())) {
            layout->autoAdd(outputObject->output());
        }

        auto restoreOutput = [this, outputObject = QPointer<Output>(outputObject)] {
            if (!outputObject || !outputObject->output()
                || !m_outputs.contains(outputObject)) {
                return;
            }

            auto *output = outputObject->output();
            auto *config = outputObject->config();
            if (!hasSavedOutputState(config)) {
                return;
            }

            const int width = static_cast<int>(config->width());
            const int height = static_cast<int>(config->height());
            const int refresh = static_cast<int>(config->refresh());
            const double scale = config->scale();
            const qlonglong transform = config->transform();
            if (width <= 0 || height <= 0 || refresh <= 0 || scale <= 0.0
                || transform < WL_OUTPUT_TRANSFORM_NORMAL
                || transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
                qCWarning(lcTlOutput) << "Ignoring invalid saved extension state for"
                                      << output->name();
                return;
            }

            if (auto *layout = m_rootContainer->outputLayout()) {
                layout->move(output,
                             QPoint(static_cast<int>(config->x()),
                                    static_cast<int>(config->y())));
            }

            WOutputStateGuard state;
            wlr_output_state_set_enabled(state.get(), true);
            if (auto *mode = closestOutputMode(output, width, height, refresh)) {
                wlr_output_state_set_mode(state.get(), mode);
            } else {
                wlr_output_state_set_custom_mode(state.get(), width, height, refresh);
            }
            wlr_output_state_set_adaptive_sync_enabled(
                state.get(), config->adaptiveSyncEnabled());
            wlr_output_state_set_transform(
                state.get(), static_cast<wl_output_transform>(transform));
            wlr_output_state_set_scale(state.get(), scale);
            if (!wlr_output_commit_state(output->handle(), state.get())) {
                qCCritical(lcTlOutput) << "Failed to restore extension state for"
                                       << output->name();
            }
        };

        auto *config = outputObject->config();
        if (config && config->isInitializeSucceeded()) {
            restoreOutput();
        }
    }

    if (!preserveSingleOutputConfig) {
        storeSingleOutputConfig();
    }

    Output *primaryOutput = findOutputById(m_config->primaryOutputId());
    if (!primaryOutput) {
        primaryOutput = m_outputs.constFirst();
    }
    m_rootContainer->setPrimaryOutput(primaryOutput);
    const auto surfaces = workspaceSurfaces();
    if (!surfaces.isEmpty()) {
        moveSurfaces(surfaces, primaryOutput, nullptr);
    }
}

void OutputManager::restoreInitialOutputConfiguration()
{
    const QString singleOutputId = m_config->singleOutputId();
    if (!singleOutputId.isEmpty()) {
        if (findOutputById(singleOutputId)) {
            restoreConfiguredSingleOutput(workspaceSurfaces(), true);
            return;
        }

        clearSingleOutputConfig();
        restoreExtensionModeFromConfig();
        return;
    }

    if (m_config->createCopyOutput()) {
        if (m_outputs.size() >= 2 && restoreConfiguredCopyMode()) {
            return;
        }

        const QStringList copyOutputs = copyOutputIds();
        if (copyOutputs.size() == 1 && findOutputById(copyOutputs.constFirst())) {
            restoreExtensionModeFromConfig(true);
            return;
        }

        qCWarning(lcTlOutput) << "Clearing invalid initial copy-output configuration";
        storeCopyOutputConfig(false);
        restoreExtensionModeFromConfig();
        return;
    }

    restoreExtensionModeFromConfig();
    restorePrimaryOutput();
}

void OutputManager::handleCopyModeOutputDisable(Output *affectedOutput)
{
    const int affectedIndex = m_outputs.indexOf(affectedOutput);
    if (affectedIndex < 0) {
        qCWarning(lcTlCore) << "Disabled output not found in OutputManager";
        return;
    }

    storeCopyOutputConfig(false);
    m_mode = Mode::Extension;
    Q_EMIT modeChanged();

    Output *primaryCandidate = nullptr;
    const auto surfaces = workspaceSurfaces(affectedOutput);
    for (int i = 0; i < m_outputs.size(); ++i) {
        if (i == affectedIndex) {
            continue;
        }

        Output *copyOutput = m_outputs.at(i);
        removeFromRootContainer(copyOutput);
        Output *normalOutput = createNormalOutput(copyOutput->output());
        normalOutput->enable();
        saveCurrentConfig(normalOutput);
        copyOutput->deleteLater();
        m_outputs.replace(i, normalOutput);
        if (!primaryCandidate) {
            primaryCandidate = normalOutput;
        }
    }

    if (primaryCandidate) {
        if (!surfaces.isEmpty()) {
            moveSurfaces(surfaces, primaryCandidate, affectedOutput);
        }
        m_rootContainer->setPrimaryOutput(primaryCandidate);
    }
}

void OutputManager::createVirtualOutput(VirtualOutputInterfaceV1 *interface)
{
    const QStringList requestedOutputs = interface->outputList();
    if (requestedOutputs.size() < 2) {
        interface->sendError(VirtualOutputInterfaceV1::INVALID_SCREEN_NUMBER,
                             "The number of screens applying for copy mode is less than 2!");
        return;
    }

    Output *mirrorOutput = findOutputByName(requestedOutputs.constFirst());
    for (const auto &outputName : std::as_const(requestedOutputs)) {
        auto *output = findOutputByName(outputName);
        if (!output) {
            interface->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT,
                                 outputName + " does not exist!");
            return;
        }
        if (!output->isSource()) {
            interface->sendError(
                VirtualOutputInterfaceV1::INVALID_OUTPUT,
                output->output()->name() + " is already a copy screen, invalid setting!");
            return;
        }
    }

    for (int i = 0; i < m_outputs.size(); ++i) {
        Output *currentOutput = m_outputs.at(i);
        if (currentOutput == mirrorOutput
            || !requestedOutputs.contains(currentOutput->output()->name())) {
            continue;
        }

        if (m_rootContainer->primaryOutput() == currentOutput) {
            m_rootContainer->setPrimaryOutput(mirrorOutput);
        }

        removeFromRootContainer(currentOutput);
        Output *replacement = createCopyOutput(currentOutput->output(), mirrorOutput);
        currentOutput->deleteLater();
        m_outputs.replace(i, replacement);
        m_rootContainer->addOutput(replacement);
        replacement->enable();
    }

    m_mode = Mode::Copy;
    QStringList requestedOutputIds;
    requestedOutputIds.reserve(requestedOutputs.size());
    for (const auto &outputName : std::as_const(requestedOutputs)) {
        if (auto *output = findOutputByName(outputName)) {
            requestedOutputIds.append(output->getOutputId());
        }
    }
    storeCopyOutputConfig(true, interface->name(), requestedOutputIds);
    moveSurfaces(workspaceSurfaces(), mirrorOutput, nullptr);
}

void OutputManager::destroyVirtualOutput(VirtualOutputInterfaceV1 *interface)
{
    const QString targetName = interface->outputList().at(0);
    if (!std::any_of(m_outputs.cbegin(),
                     m_outputs.cend(),
                     [&targetName](const Output *output) {
                         return output->output()->name() == targetName;
                     })) {
        interface->sendError(
            VirtualOutputInterfaceV1::INVALID_OUTPUT,
            QString("Target output %1 does not exist!").arg(targetName));
        return;
    }

    for (int i = 0; i < m_outputs.size(); ++i) {
        Output *currentOutput = m_outputs.at(i);
        if (currentOutput->output()->name() == targetName) {
            continue;
        }

        removeFromRootContainer(currentOutput);
        Output *replacement = createNormalOutput(currentOutput->output());
        replacement->enable();
        saveCurrentConfig(replacement);
        currentOutput->deleteLater();
        m_outputs.replace(i, replacement);
    }
    m_mode = Mode::Extension;
    storeCopyOutputConfig(false);
}

void OutputManager::setGamma(wlr_gamma_control_manager_v1_set_gamma_event *event)
{
    auto *output = event->output;
    size_t rampSize = 0;
    uint16_t *red = nullptr;
    uint16_t *green = nullptr;
    uint16_t *blue = nullptr;
    wlr_gamma_control_v1 *gammaControl = event->control;
    if (gammaControl) {
        rampSize = gammaControl->ramp_size;
        red = gammaControl->table;
        green = gammaControl->table + gammaControl->ramp_size;
        blue = gammaControl->table + 2 * gammaControl->ramp_size;
    }

    WOutputStateGuard state;
    wlr_color_transform *colorTransform = nullptr;
    if (gammaControl) {
        colorTransform = wlr_color_transform_init_lut_3x1d(
            rampSize, red, green, blue);
        if (!colorTransform) {
            qCWarning(lcTlCore) << "Failed to create color transform for gamma LUT!";
            wlr_gamma_control_v1_send_failed_and_destroy(gammaControl);
            return;
        }
    }
    wlr_output_state_set_color_transform(state.get(), colorTransform);
    wlr_color_transform_unref(colorTransform);
    if (!wlr_output_commit_state(output, state.get())) {
        qCCritical(lcTlCore, "commit failed on output  %s", output->name);
        qCWarning(lcTlCore) << "Failed to set gamma lut!";
        wlr_gamma_control_v1_send_failed_and_destroy(gammaControl);
    }
}

void OutputManager::setPowerMode(wlr_output_power_v1_set_mode_event *event)
{
    auto *output = event->output;
    WOutputStateGuard state;

    switch (event->mode) {
    case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
        if (m_powerOffOutputs.contains(output) || !output->enabled) {
            return;
        }
        wlr_output_state_set_enabled(state.get(), false);
        if (!wlr_output_commit_state(output, state.get())) {
            qCCritical(lcTlCore, "commit failed on output %s", output->name);
            return;
        }
        m_powerOffOutputs.insert(output);
        break;
    case ZWLR_OUTPUT_POWER_V1_MODE_ON:
        if (!m_powerOffOutputs.remove(output)) {
            return;
        }
        wlr_output_state_set_enabled(state.get(), true);
        if (!wlr_output_commit_state(output, state.get())) {
            qCCritical(lcTlCore, "commit failed on output %s", output->name);
            m_powerOffOutputs.insert(output);
        }
        break;
    }
}

void OutputManager::testOrApplyConfiguration(wlr_output_configuration_v1 *config,
                                             bool onlyTest)
{
    QList<WOutputState> states = m_outputManagementProtocol->stateListPending(config);

    const auto enabledOutputCount = std::count_if(
        states.cbegin(), states.cend(), [](const WOutputState &state) { return state.enabled; });
    const bool allEnabledOutputsOverlap = enabledOutputCount > 1
        && std::all_of(states.cbegin(), states.cend(), [](const WOutputState &state) {
               return !state.enabled || (state.x == 0 && state.y == 0);
           });
    const auto currentlyEnabledOutputCount = std::count_if(
        states.cbegin(), states.cend(), [](const WOutputState &state) {
            return state.output->isEnabled();
        });
    const bool expandingFromSingleOutput =
        currentlyEnabledOutputCount == 1 && enabledOutputCount > 1;

    if (m_mode == Mode::Extension
        && (allEnabledOutputsOverlap || expandingFromSingleOutput)) {
        QList<WOutputState> restoredStates = states;
        bool configsValid = true;
        bool hasNonZeroPosition = false;

        for (auto &state : restoredStates) {
            if (!state.enabled) {
                continue;
            }

            Output *output = outputFor(state.output);
            OutputConfig *outputConfig = output ? output->config() : nullptr;
            if (!outputConfig || !outputConfig->isInitializeSucceeded()
                || !hasSavedOutputState(outputConfig)) {
                configsValid = false;
                break;
            }

            const int width = static_cast<int>(outputConfig->width());
            const int height = static_cast<int>(outputConfig->height());
            const int refresh = static_cast<int>(outputConfig->refresh());
            if (width <= 0 || height <= 0 || refresh <= 0) {
                configsValid = false;
                break;
            }

            state.x = static_cast<int32_t>(outputConfig->x());
            state.y = static_cast<int32_t>(outputConfig->y());
            hasNonZeroPosition |= state.x != 0 || state.y != 0;
            state.mode = closestOutputMode(state.output, width, height, refresh);
            if (!state.mode) {
                configsValid = false;
                break;
            }
        }

        if (configsValid && hasNonZeroPosition) {
            states = std::move(restoredStates);
        }
    }

    if (onlyTest) {
        bool ok = true;
        for (const auto &state : std::as_const(states)) {
            WOutputViewport *viewport = ownOutputViewport(state.output);
            if (!viewport) {
                ok = false;
                continue;
            }

            WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
            if (!renderWindow) {
                ok = false;
                continue;
            }
            WOutputStateGuard newState;
            wlr_output_state_set_enabled(newState.get(), state.enabled);
            if (state.enabled) {
                if (state.mode) {
                    wlr_output_state_set_mode(newState.get(), state.mode);
                } else {
                    wlr_output_state_set_custom_mode(
                        newState.get(),
                        state.customModeSize.width(),
                        state.customModeSize.height(),
                        state.customModeRefresh);
                }
                wlr_output_state_set_adaptive_sync_enabled(
                    newState.get(), state.adaptiveSyncEnabled);
                wlr_output_state_set_transform(
                    newState.get(), static_cast<wl_output_transform>(state.transform));
                wlr_output_state_set_scale(newState.get(), state.scale);
            }
            ok &= wlr_output_test_state(state.output->handle(), newState.get());
        }

        m_outputManagementProtocol->sendResult(config, ok);
        return;
    }

    if (m_pendingOutputConfig.config) {
        m_outputManagementProtocol->sendResult(m_pendingOutputConfig.config, false);
    }

    if (m_mode == Mode::Copy) {
        for (const auto &state : std::as_const(states)) {
            if (!state.enabled) {
                Output *affectedOutput = outputFor(state.output);
                if (affectedOutput && affectedOutput == m_rootContainer->primaryOutput()) {
                    handleCopyModeOutputDisable(affectedOutput);
                    break;
                }
            }
        }
    }

    m_pendingOutputConfig.config = config;
    m_pendingOutputConfig.states = states;
    m_pendingOutputConfig.pendingCommits = 0;
    m_pendingOutputConfig.allSuccess = true;

    if (m_initialOutputScanFinished && !m_config->singleOutputId().isEmpty()) {
        const QString singleOutputId = m_config->singleOutputId();
        for (const auto &state : std::as_const(states)) {
            if (state.enabled && !state.output->isEnabled()) {
                Output *output = outputFor(state.output);
                if (output && output->getOutputId() != singleOutputId) {
                    clearSingleOutputConfig();
                    enableAllOutputs();
                    break;
                }
            }
        }
    }

    if (m_mode == Mode::Copy) {
        for (int i = 0; i < m_outputs.size(); ++i) {
            Output *copyOutput = m_outputs.at(i);
            if (copyOutput->isSource()) {
                continue;
            }
            removeFromRootContainer(copyOutput);
            Output *normalOutput = createNormalOutput(copyOutput->output());
            copyOutput->deleteLater();
            m_outputs.replace(i, normalOutput);
        }
    }

    if (m_mode != Mode::Extension) {
        m_mode = Mode::Extension;
        Q_EMIT modeChanged();
    }
    clearCopyModeRestoreIntent();

    for (const auto &state : std::as_const(states)) {
        Output *outputObject = outputFor(state.output);
        if (!outputObject) {
            continue;
        }
        if (!state.enabled && state.output->isEnabled()) {
            onScreenDisabled(outputObject, workspaceSurfaces(outputObject));
        } else if (state.enabled && !state.output->isEnabled()) {
            clearCopyModeRestoreIntent();
        }
    }

    for (const auto &state : std::as_const(states)) {
        Output *output = outputFor(state.output);
        if (!output) {
            continue;
        }

        WOutputViewport *viewport = ownOutputViewport(state.output);
        if (!viewport) {
            m_outputManagementProtocol->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
        if (!renderWindow) {
            qCWarning(lcTlCore) << "No renderWindow for output" << state.output->name();
            m_outputManagementProtocol->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        if (state.enabled) {
            auto *layout = m_rootContainer->outputLayout();
            if (!layout || !m_rootContainer->outputs().contains(output)) {
                qCWarning(lcTlCore)
                    << "Cannot apply enabled output configuration; output is not in root container"
                    << state.output->name();
                m_outputManagementProtocol->sendResult(config, false);
                m_pendingOutputConfig = {};
                return;
            }
            if (layout->outputs().contains(state.output)) {
                layout->move(state.output, QPoint(state.x, state.y));
            }
        }

        auto outputHelper = renderWindow->getOutputHelper(viewport);
        if (!outputHelper) {
            qCWarning(lcTlCore) << "No output helper for viewport" << viewport;
            m_outputManagementProtocol->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputHelper::ExtraState extraState;
        wlr_output_state_set_enabled(extraState.get(), state.enabled);
        if (state.enabled) {
            if (state.mode) {
                wlr_output_state_set_mode(extraState.get(), state.mode);
            } else {
                wlr_output_state_set_custom_mode(
                    extraState.get(),
                    state.customModeSize.width(),
                    state.customModeSize.height(),
                    state.customModeRefresh);
            }
            wlr_output_state_set_scale(extraState.get(), state.scale);
            wlr_output_state_set_transform(
                extraState.get(), static_cast<wl_output_transform>(state.transform));
            wlr_output_state_set_adaptive_sync_enabled(
                extraState.get(), state.adaptiveSyncEnabled);

            if (auto *outputItem = qobject_cast<WOutputItem *>(viewport->parentItem())) {
                QMetaObject::invokeMethod(
                    outputItem,
                    "setTransform",
                    Q_ARG(QVariant,
                          QVariant::fromValue(static_cast<WOutput::Transform>(state.transform))));
            }
        }

        if (!outputHelper->setExtraState(extraState)) {
            qCWarning(lcTlCore) << "Failed to set extra state for output"
                                << state.output->name();
            m_outputManagementProtocol->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        auto pendingConfig = m_pendingOutputConfig.config;
        const bool enabled = state.enabled;
        const QPoint outputPosition(state.x, state.y);
        QPointer<OutputManager> self(this);
        outputHelper->scheduleCommitJob(
            [self,
             pendingConfig,
             extraState,
             renderWindow,
             viewport,
             output = QPointer<WOutput>(state.output),
             outputPosition,
             enabled](bool success, WOutputHelper::ExtraState committedState) {
                if (!self) {
                    return;
                }

                if (committedState == extraState) {
                    if (success && output) {
                        auto *layout = self->m_rootContainer->outputLayout();
                        if (layout && enabled && !layout->outputs().contains(output)) {
                            layout->add(output, outputPosition);
                        } else if (layout && !enabled
                                   && layout->outputs().contains(output)) {
                            layout->remove(output);
                        }
                    }
                    self->onOutputCommitFinished(pendingConfig, success);
                    if (success && committedState) {
                        const bool wasStateOnlyCommit =
                            (committedState->committed
                             & (WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_SCALE
                                | WLR_OUTPUT_STATE_TRANSFORM | WLR_OUTPUT_STATE_ENABLED))
                            && !(committedState->committed & WLR_OUTPUT_STATE_BUFFER);
                        const bool isDisable =
                            (committedState->committed & WLR_OUTPUT_STATE_ENABLED)
                            && !committedState->enabled;
                        if (wasStateOnlyCommit && !isDisable) {
                            renderWindow->update(viewport);
                        }
                    }
                } else {
                    qCWarning(lcTlCore)
                        << "Commit callback received unexpected state pointer!"
                        << "Expected:" << extraState.get()
                        << "Got:" << committedState.get();
                    self->onOutputCommitFinished(pendingConfig, false);
                }
            },
            WOutputHelper::AfterCommitStage);
        m_pendingOutputConfig.pendingCommits++;
        renderWindow->update(viewport);

        if (state.enabled && !state.output->isEnabled()) {
            renderWindow->render(viewport, true);
        }
    }
}

void OutputManager::onOutputCommitFinished(wlr_output_configuration_v1 *config,
                                           bool success)
{
    if (!config || config != m_pendingOutputConfig.config) {
        return;
    }

    if (!success) {
        m_pendingOutputConfig.allSuccess = false;
    }

    m_pendingOutputConfig.pendingCommits--;
    if (m_pendingOutputConfig.pendingCommits != 0) {
        return;
    }

    const bool ok = m_pendingOutputConfig.allSuccess;
    if (ok) {
        storeSingleOutputConfig();
        storeCopyOutputConfig(false);

        const auto enabledOutputCount = std::count_if(
            m_pendingOutputConfig.states.cbegin(),
            m_pendingOutputConfig.states.cend(),
            [](const WOutputState &state) { return state.enabled; });

        for (const WOutputState &state : std::as_const(m_pendingOutputConfig.states)) {
            auto *output = outputFor(state.output);
            if (!output) {
                continue;
            }
            if (state.enabled) {
                onScreenEnabled(output);
            }

            auto *outputConfig = output->config();
            const bool preservePosition = state.enabled && enabledOutputCount == 1;
            if (!state.enabled || !outputConfig) {
                continue;
            }
            if (!preservePosition) {
                outputConfig->setX(state.x);
                outputConfig->setY(state.y);
            }
            outputConfig->setWidth(
                state.mode ? state.mode->width : state.customModeSize.width());
            outputConfig->setHeight(
                state.mode ? state.mode->height : state.customModeSize.height());
            outputConfig->setRefresh(
                state.mode ? state.mode->refresh : state.customModeRefresh);
            outputConfig->setTransform(output->output()->handle()->transform);
            outputConfig->setScale(state.scale);
            outputConfig->setAdaptiveSyncEnabled(state.adaptiveSyncEnabled);
        }
    }

    m_outputManagementProtocol->sendResult(config, ok, m_pendingOutputConfig.states);
    m_pendingOutputConfig = {};
}

bool OutputManager::shouldRestoreCopyMode(int availableOutputCount) const
{
    return m_config->singleOutputId().isEmpty()
        && m_config->createCopyOutput() && availableOutputCount >= 2;
}

bool OutputManager::restoreConfiguredSingleOutput(const QList<SurfaceWrapper *> &surfaces, bool updatePrimaryOutputConfig)
{
    if (!m_rootContainer || m_config->singleOutputId().isEmpty()) {
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
            if (auto *layout = m_rootContainer->outputLayout();
                output->output()->isEnabled()
                && layout
                && !layout->outputs().contains(output->output())) {
                layout->autoAdd(output->output());
            }
            continue;
        }
        if (output->output()->isEnabled()) {
            WOutputStateGuard state;
            wlr_output_state_set_enabled(state.get(), false);
            const bool committed = wlr_output_commit_state(output->output()->handle(), state.get());
            if (!committed) {
                qCWarning(lcTlOutput) << "Failed to disable non-selected output while restoring single-output display"
                                      << output->output()->name();
            }
        }
        if (auto *layout = m_rootContainer->outputLayout();
            !output->output()->isEnabled()
            && layout
            && layout->outputs().contains(output->output())) {
            layout->remove(output->output());
        }
    }

    m_rootContainer->setPrimaryOutput(singleOutput, updatePrimaryOutputConfig);
    m_rootContainer->moveSurfacesToOutput(surfaces, singleOutput, nullptr);
    return true;
}

void OutputManager::restorePrimaryOutput()
{
    if (!m_rootContainer) {
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
    return deserializeOutputIds(m_config->copyOutputOutputs());
}

QStringList OutputManager::currentOutputIds(Output *primaryOutput) const
{
    QStringList ids;
    if (primaryOutput) {
        ids.append(primaryOutput->getOutputId());
    }
    if (!m_rootContainer) {
        return ids;
    }
    for (auto *output : std::as_const(m_rootContainer->outputs())) {
        if (output && output != primaryOutput) {
            ids.append(output->getOutputId());
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
    m_config->setSingleOutputId(QString());
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
                singleOutputId = output->getOutputId();
            } else {
                singleOutputId.clear();
                break;
            }
        }
    }

    m_config->setSingleOutputId(singleOutputId);
}

void OutputManager::storeCopyOutputConfig(bool enabled,
                                          const QString &name,
                                          const QStringList &outputIds)
{
    if (!enabled) {
        const QString oldName = m_config->copyOutputName();
        m_config->setCreateCopyOutput(false);
        m_config->setCopyOutputName(QString());
        m_config->setCopyOutputOutputs(QStringLiteral("[]"));
        if (m_virtualOutputInterface && !oldName.isEmpty())
            m_virtualOutputInterface->removeVirtualOutput(oldName);
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
    if (m_virtualOutputInterface)
        m_virtualOutputInterface->updateVirtualOutput(configName, outputNamesFromIds(outputIds));
}

void OutputManager::setCopyModeStored(bool enabled)
{
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

void OutputManager::markScreenAsPrimaryIntent(Output *output)
{
    const QString id = output->getOutputId();
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

    const QString id = output->getOutputId();
    const bool wasPrimary = m_primaryRestoreIntents.value(id);
    const bool hasPrimaryOutput = m_rootContainer->primaryOutput() != nullptr;

    if (id == m_config->singleOutputId()) {
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
    const bool wasPrimaryBeforeRemoval = m_primaryRestoreIntents.value(output->getOutputId());

    if (output->getOutputId() == m_config->singleOutputId()) {
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

    const QString id = output->getOutputId();
    const bool wasPrimary = m_primaryRestoreIntents.value(id);
    if (wasPrimary && m_mode == Mode::Extension && m_rootContainer->primaryOutput()) {
        restoreScreenAsPrimary(output);
    }

    m_primaryRestoreIntents.remove(id);
}
