// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscopedvalue.h>
#include "outputmanager.h"

#include "common/treelandlogging.h"
#include "dconfig/dconfigmanager.h"
#include "core/rootsurfacecontainer.h"
#include "modules/output-group/outputgroupmanagerinterfacev1.h"
#include "output.h"
#include "outputconfig.hpp"
#include "session/session.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "wallpaper/wallpapermanager.h"
#include "workspace/workspace.h"
#include <QCursor>
#include <QPointer>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QUuid>
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

void OutputManager::setOutputGroupInterface(OutputGroupManagerInterfaceV1 *interface)
{
    Q_ASSERT(!m_outputGroupManager);
    m_outputGroupManager = interface;
    if (m_outputGroupManager) {
        m_outputGroupManager->setHost(this);
    }
}

void OutputManager::setGammaControlManager(wlr_gamma_control_manager_v1 *manager)
{
    m_gammaControlListenerOwner.teardown();
    m_gammaControlListenerOwner.listeners()->add(
        &manager->events.set_gamma, this, &OutputManager::setGamma);
    m_gammaControlListenerOwner.listeners()->add(
        &manager->events.destroy, this, [this](void *) {
            m_gammaControlListenerOwner.removeListeners(&m_gammaControlListenerOwner);
        });
}

void OutputManager::setOutputPowerManager(wlr_output_power_manager_v1 *manager)
{
    m_outputPowerListenerOwner.teardown();
    m_outputPowerListenerOwner.listeners()->add(
        &manager->events.set_mode, this, &OutputManager::setPowerMode);
    m_outputPowerListenerOwner.listeners()->add(
        &manager->events.destroy, this, [this](void *) {
            m_outputPowerListenerOwner.removeListeners(&m_outputPowerListenerOwner);
        });
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
    auto *outputConfig = DConfigManager::instance()->outputConfig(Output::getOutputId(output->handle()));
    if (outputConfig && outputConfig->uuid().isEmpty()) {
        outputConfig->setUuid(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    allowNonDrmOutputAutoChangeMode(output);
    Output *o = nullptr;
    const bool isInitialOutput = !m_initialOutputScanFinished;
    qCInfo(lcTlOutput) << "Output added" << output->name()
                       << "id:" << Output::getOutputId(output->handle())
                       << "scan complete:" << m_initialOutputScanFinished
                       << "mode:" << static_cast<int>(m_mode);

    // Every output enters as a normal output; the group reconciler turns it
    // into a mirror when a group claims it. The initial scan collects the
    // normal outputs first and copy mode is applied once backend start has
    // reported all outputs and the persisted groups are known.
    o = createNormalOutput(output);
    m_outputs.append(o);
    const bool outputRegistered = ensureInRootContainer(o);
    if (!outputRegistered) {
        qCWarning(lcTlOutput) << "Failed to register output in root container" << output->name();
    }
    if (m_initialOutputScanFinished && outputRegistered && !output->isEnabled()) {
        o->enable();
    }
    if (m_initialOutputScanFinished) {
        onScreenAdded(o, workspaceSurfaces());
        notifyOutputGroupsChanged();
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
                && m_config->singleOutputId().isEmpty();
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

    m_pendingOutputs.insert(output, config);
    connect(output, &QObject::destroyed, this, [this, output] {
        if (m_pendingOutputs.remove(output)) {
            finishInitialOutputScanIfReady();
        }
    });

    connect(config,
            &OutputConfig::configInitializeSucceed,
            this,
            &OutputManager::continueOutputAdded,
            Qt::SingleShotConnection);
    connect(config,
            &OutputConfig::configInitializeFailed,
            this,
            &OutputManager::continueOutputAdded,
            Qt::SingleShotConnection);
}

void OutputManager::continueOutputAdded()
{
    auto *config = qobject_cast<OutputConfig *>(sender());
    if (!config) {
        return;
    }

    const auto outputs = m_pendingOutputs.keys(config);
    for (auto *output : outputs) {
        if (m_pendingOutputs.remove(output)) {
            processOutputAdded(output);
        }
    }
    finishInitialOutputScanIfReady();
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

    output->removeListeners(&m_outputStateListenerOwner);
    auto index = indexOf(output);
    Q_ASSERT(index >= 0);
    const auto o = m_outputs.takeAt(index);

    const auto &surfaces = workspaceSurfaces(o);
    removeFromRootContainer(o);
    const bool removedConfiguredSingleOutput = onScreenRemoved(o, surfaces);
    if (removedConfiguredSingleOutput) {
        // Keep the unavailable output as the configured single-output target.
        // The remaining outputs are enabled only as a temporary fallback.
        restoreExtensionModeFromConfig(true);
    }
    // Losing a member output never dissolves its group: the effective state
    // drops the output, and the configured state is kept so copy mode resumes
    // when the output comes back.
    notifyOutputGroupsChanged();

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
    if (!m_outputGroupManager) {
        return;
    }

    // The menu bar toggle keeps working, but copy mode is driven by the output
    // group protocol: it creates one group with the primary output as source
    // and every other output as a mirror. Only the group this entry point
    // created is dissolved again, so groups configured by other clients (the
    // display settings panel) are left alone.
    // Read the uuid before dissolving: dissolving may replace output objects.
    const QString primaryUuid = m_rootContainer->primaryOutput()
        ? m_rootContainer->primaryOutput()->uuid()
        : QString();

    if (!m_menuBarGroupName.isEmpty()) {
        m_outputGroupManager->dissolveGroup(m_menuBarGroupName);
        m_menuBarGroupName.clear();
    }

    if (mode != Mode::Copy || primaryUuid.isEmpty()) {
        return;
    }

    QStringList mirrorUuids;
    for (auto *output : std::as_const(m_outputs)) {
        if (output && output->uuid() != primaryUuid && !output->uuid().isEmpty()) {
            mirrorUuids.append(output->uuid());
        }
    }
    m_menuBarGroupName = m_outputGroupManager->createGroup(primaryUuid, mirrorUuids);
}

void OutputManager::allowNonDrmOutputAutoChangeMode(
    WAYLIB_SERVER_NAMESPACE::WOutput *output)
{
    output->listeners(&m_outputStateListenerOwner)->add(
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

bool OutputManager::isOutputAvailable(const QString &uuid) const
{
    Output *output = findOutputByUuid(uuid);
    return output && output->output() && output->output()->isEnabled();
}

bool OutputManager::canReconfigureOutputs() const
{
    return m_initialOutputScanFinished;
}

Output *OutputManager::findOutputByUuid(const QString &uuid) const
{
    if (uuid.isEmpty()) {
        return nullptr;
    }

    for (auto *output : std::as_const(m_outputs)) {
        if (output && output->output() && output->uuid() == uuid) {
            return output;
        }
    }
    return nullptr;
}

void OutputManager::notifyOutputGroupsChanged()
{
    if (!m_outputGroupManager) {
        return;
    }

    // Availability changes are reported by the protocol owner, which then asks
    // us to apply the resulting effective group state.
    QMetaObject::invokeMethod(
        this,
        [this] {
            if (m_outputGroupManager) {
                m_outputGroupManager->notifyOutputAvailabilityChanged();
            }
        },
        Qt::QueuedConnection);
}

void OutputManager::deactivateGroupMirrors()
{
    for (int i = 0; i < m_outputs.size(); ++i)
        convertMirrorToNormal(i);
}

// Turns the mirror at INDEX back into a normal output. The replacement is
// created synchronously (not deleteLater) on purpose: the mirror and its
// replacement share one WOutputViewport, and the mirror's hardware layers stay
// attached to it until the old object is gone, which makes waylib assert when
// the new item re-attaches them. Disable first, so the layers are detached
// while the mirror still owns the viewport.
Output *OutputManager::convertMirrorToNormal(int index)
{
    if (index < 0 || index >= m_outputs.size()) {
        return nullptr;
    }

    Output *output = m_outputs.at(index);
    if (!output || !output->output() || output->isSource()) {
        return output;
    }

    auto *woutput = output->output();
    const bool wasEnabled = woutput->isEnabled();
    if (wasEnabled) {
        WOutputStateGuard disabledState;
        wlr_output_state_set_enabled(disabledState.get(), false);
        if (!wlr_output_commit_state(woutput->handle(), disabledState.get())) {
            qCWarning(lcTlOutput) << "Failed to disable output before leaving copy mode"
                                  << woutput->name();
        }
    }

    removeFromRootContainer(output);
    m_outputs.removeAt(index);
    delete output;

    Output *replacement = createNormalOutput(woutput);
    // Keep the enabled state the output had: an output switched off by DPMS or
    // by single-output display must not be woken up again by leaving copy mode.
    if (wasEnabled) {
        replacement->enable();
    }
    saveCurrentConfig(replacement);
    m_outputs.insert(index, replacement);
    return replacement;
}

// Reconciles the Output objects with the effective state of every output
// group: a group source is a normal output, its effective mirrors are copy
// outputs of that source. Outputs no group claims stay normal extension
// outputs.
//
// Known deviation from the XML: mirror outputs still expose a wl_output while
// copy mode is engaged. The protocol describes the source as the only output
// exposing a wl_output; that behaviour is tracked separately and is not part of
// this change.
void OutputManager::applyOutputGroups()
{
    if (!m_outputGroupManager || m_applyingOutputGroups) {
        return;
    }
    if (m_pendingOutputConfig.config || m_inFlightOutputCommits > 0) {
        // An output configuration is being applied: its commit jobs hold the
        // viewports of the outputs that were just converted back to normal.
        // Rebuilding them now would destroy those helpers and lose the jobs,
        // so re-apply once the last job finished.
        m_groupsDirty = true;
        return;
    }
    m_applyingOutputGroups = true;
    const QScopeGuard resetGuard([this] { m_applyingOutputGroups = false; });

    const auto groups = m_outputGroupManager->effectiveGroups();
    QHash<QString, QString> desiredMirrors; // mirror uuid -> source uuid
    QStringList desiredSources;
    for (const auto &group : std::as_const(groups)) {
        if (group.sourceUuid.isEmpty()) {
            continue;
        }
        desiredSources.append(group.sourceUuid);
        for (const auto &mirrorUuid : std::as_const(group.mirrorUuids)) {
            desiredMirrors.insert(mirrorUuid, group.sourceUuid);
        }
    }

    // Group sources are always available (the protocol filtered them), make
    // sure they are enabled and present in the layout.
    for (const auto &uuid : std::as_const(desiredSources)) {
        Output *source = findOutputByUuid(uuid);
        if (!source || !source->output()) {
            continue;
        }
        if (!source->output()->isEnabled()) {
            source->enable();
        }
        if (auto *layout = m_rootContainer->outputLayout();
            source->output()->isEnabled()
            && layout
            && !layout->outputs().contains(source->output())) {
            layout->autoAdd(source->output());
        }
    }

    // First turn every output that should not be a mirror into a normal output,
    // so that a mirror can never keep a proxy that is about to be recreated.
    for (int i = 0; i < m_outputs.size(); ++i) {
        Output *output = m_outputs.at(i);
        if (!output || !output->output()) {
            continue;
        }

        const auto mirrorIt = desiredMirrors.constFind(output->uuid());
        if (!output->isSource()) {
            Output *desiredSource = mirrorIt == desiredMirrors.cend()
                ? nullptr
                : findOutputByUuid(mirrorIt.value());
            if (desiredSource && output->proxyOutput() == desiredSource) {
                continue;
            }

            convertMirrorToNormal(i);
        }
    }

    // Then create a copy output for every effective mirror. Mirrors that pass A
    // already kept are not touched again.
    for (int i = 0; i < m_outputs.size(); ++i) {
        Output *output = m_outputs.at(i);
        if (!output || !output->output() || !output->isSource()) {
            continue;
        }

        const auto mirrorIt = desiredMirrors.constFind(output->uuid());
        if (mirrorIt == desiredMirrors.cend()) {
            continue;
        }

        Output *source = findOutputByUuid(mirrorIt.value());
        if (!source) {
            continue;
        }

        removeFromRootContainer(output);
        Output *replacement = createCopyOutput(output->output(), source);
        m_outputs.replace(i, replacement);
        // The mirror's viewport may already have re-registered the output in
        // the layout while the QML item was created.
        if (!ensureInRootContainer(replacement)) {
            qCWarning(lcTlOutput) << "Failed to register mirror output" << output->uuid();
        }
        replacement->enable();
        output->deleteLater();
    }

    // A mirror can never be the primary output.
    Output *primary = m_rootContainer->primaryOutput();
    if (!primary || !m_outputs.contains(primary) || !primary->isSource()) {
        const auto surfaces = primary ? workspaceSurfaces(primary) : QList<SurfaceWrapper *>();
        Output *fallback = findOutputByUuid(desiredSources.value(0));
        if (!fallback) {
            for (auto *candidate : std::as_const(m_outputs)) {
                if (candidate && candidate->isSource()) {
                    fallback = candidate;
                    break;
                }
            }
        }
        if (fallback) {
            m_rootContainer->setPrimaryOutput(fallback);
            if (!surfaces.isEmpty()) {
                moveSurfaces(surfaces, fallback, primary);
            }
        }
    }

    bool copyActive = false;
    for (const auto &group : std::as_const(groups)) {
        if (!group.sourceUuid.isEmpty() && !group.mirrorUuids.isEmpty()) {
            copyActive = true;
            break;
        }
    }

    const Mode newMode = copyActive ? Mode::Copy : Mode::Extension;
    if (m_mode != newMode) {
        m_mode = newMode;
        Q_EMIT modeChanged();
    }
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
    if (!singleOutputId.isEmpty() && findOutputById(singleOutputId)) {
        restoreConfiguredSingleOutput(workspaceSurfaces(), true);
    } else {
        if (!singleOutputId.isEmpty()) {
            clearSingleOutputConfig();
        }

        restoreExtensionModeFromConfig();
        restorePrimaryOutput();
    }

    // Persisted output groups come from the output group protocol; applying
    // them here restores copy mode for every configured group.
    notifyOutputGroupsChanged();
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

    notifyOutputGroupsChanged();
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

    // The output-management protocol configures the outputs itself, so copy
    // mode is torn down here and restored from the groups once the commit
    // finished (see onOutputCommitFinished). The reconcile also runs on every
    // error path below, so the protocol state can never claim a mirror that is
    // not rendered anymore.
    const QScopeGuard reconcileOnExit([this] { notifyOutputGroupsChanged(); });

    deactivateGroupMirrors();

    for (const auto &state : std::as_const(states)) {
        Output *outputObject = outputFor(state.output);
        if (!outputObject) {
            continue;
        }
        if (!state.enabled && state.output->isEnabled()) {
            onScreenDisabled(outputObject, workspaceSurfaces(outputObject));
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

                const bool lastInFlight = (--self->m_inFlightOutputCommits <= 0);
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

                if (lastInFlight && self->m_groupsDirty) {
                    // Recompute instead of applying the cached effective state:
                    // the commit that just finished may have changed whether an
                    // output is available (e.g. the client disabled a mirror
                    // through wlr-output-management).
                    self->m_groupsDirty = false;
                    self->notifyOutputGroupsChanged();
                }
            },
            WOutputHelper::AfterCommitStage);
        m_pendingOutputConfig.pendingCommits++;
        m_inFlightOutputCommits++;
        renderWindow->update(viewport);

        if (state.enabled && !state.output->isEnabled()) {
            renderWindow->render(viewport, true);
        }
    }

    if (m_pendingOutputConfig.pendingCommits == 0) {
        // Nothing was scheduled (empty configuration or no matching output), so
        // no commit callback will ever finish this apply.
        m_outputManagementProtocol->sendResult(config, true);
        m_pendingOutputConfig = {};
        notifyOutputGroupsChanged();
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

    // Even a failed apply may have disabled outputs while copy mode was already
    // torn down; re-applying the groups keeps the reported state truthful.
    notifyOutputGroupsChanged();
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

    if (isCurrentPrimary) {
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

    notifyOutputGroupsChanged();
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
