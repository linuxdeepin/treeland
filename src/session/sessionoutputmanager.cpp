// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionoutputmanager.h"
#include "desktop/desktopenvironment.h"
#include "dconfig/dconfigmanager.h"
#include "sessionoutputchannel.h"
#include "surface/surfacewrapper.h"
#include "common/treelandlogging.h"
#include <wlayersurface.h>
#include <wlr_all.h>
#include "core/rootsurfacecontainer.h"
#include "output/outputmanager.h"
#include "output/output.h"
#include "outputconfig.hpp"
#include "treelandconfig.hpp"
#include <woutput.h>
#include <wbackend.h>
#include <utility>
#include <QSet>
#include <cmath>
#include <algorithm>

SessionOutputManager::SessionOutputManager(TreelandConfig *config, ConfigProvider outputConfiguration,
                                         DesktopSurfaceModel *surfaces, SessionOutputChannel *channel,
                                         OutputManager *hardware, WBackend *backend,
                                         WOutputManagerV1 *protocol, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_outputConfiguration(std::move(outputConfiguration))
    , m_surfaces(surfaces)
    , m_channel(channel)
    , m_hardware(hardware)
    , m_protocol(protocol)
{
    connect(channel, &SessionOutputChannel::connected, this, &SessionOutputManager::refresh);
    // Drop removed outputs synchronously before their WOutput wrappers die;
    // queued state publication must never retain a stale output pointer.
    connect(backend, &WBackend::outputRemoved,
            m_protocol, &WOutputManagerV1::removeOutput);
    connect(m_protocol, &WOutputManagerV1::requestTestOrApply,
            this, &SessionOutputManager::requestConfiguration);
    connect(m_config, &TreelandConfig::configInitializeSucceed,
            this, &SessionOutputManager::refresh);
    connect(m_config, &TreelandConfig::configInitializeFailed,
            this, &SessionOutputManager::refresh);
    connect(m_hardware, &OutputManager::configurationChanged,
            this, &SessionOutputManager::refresh, Qt::QueuedConnection);
    connect(m_hardware, &OutputManager::outputsChanged,
            this, &SessionOutputManager::refresh, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, &SessionOutputManager::refresh, Qt::QueuedConnection);
}

OutputConfig *SessionOutputManager::config(const QString &outputId)
{
    if (auto *config = m_configs.value(outputId))
        return config;
    auto *config = m_outputConfiguration(outputId);
    connect(config, &OutputConfig::configInitializeSucceed,
            this, &SessionOutputManager::refresh);
    connect(config, &OutputConfig::configInitializeFailed,
            this, &SessionOutputManager::refresh);
    connect(config, &OutputConfig::valueChanged,
            this, &SessionOutputManager::publishConfiguration, Qt::QueuedConnection);
    m_configs.insert(outputId, config);
    return config;
}

void SessionOutputManager::setScale(WOutput *target, qreal scale)
{
    m_channel->setScale(target, scale);
}

void SessionOutputManager::refresh()
{
    if (!configurationReady(m_config))
        return;
    bool ready = true;
    for (auto *output : m_hardware->outputs())
        ready &= configurationReady(config(output->getOutputId()));
    if (!ready)
        return;
    publishConfiguration();
}

qreal SessionOutputManager::effectiveScale() const
{
    qreal scale = 1.0;
    for (auto *output : m_hardware->outputs()) {
        if (output->output()->isEnabled())
            scale = qMax(scale, qreal(output->output()->scale()));
    }
    return scale;
}

qreal SessionOutputManager::scaleAt(const QPointF &position)
{
    for (auto *output : m_hardware->outputs()) {
        if (outputState(output)->geometry().contains(position))
            return output->output()->scale();
    }
    return effectiveScale();
}

QPointF SessionOutputManager::defaultPosition(Output *target)
{
    qreal x = 0;
    for (auto *output : m_hardware->outputs()) {
        if (output == target)
            break;
        const auto previous = outputState(output)->geometry();
        x = qMax(x, previous.right());
    }
    return QPointF(x, 0);
}

QString SessionOutputManager::primaryOutputName() const
{
    const auto id = m_config->primaryOutputId();
    for (auto *output : m_hardware->outputs()) {
        if (output->getOutputId() == id)
            return output->output()->name();
    }
    const auto &outputs = m_hardware->outputs();
    return outputs.isEmpty() ? QString() : outputs.first()->output()->name();
}

void SessionOutputManager::setPrimaryOutput(const QString &name)
{
    for (auto *output : m_hardware->outputs()) {
        if (output->output()->name() != name)
            continue;
        m_channel->setPrimary(output);
        return;
    }
}

void SessionOutputManager::applyColor(Output *output, qreal brightness, uint32_t temperature,
                                     std::function<void(Output::CommitColorResult)> completed)
{
    m_channel->applyColor(output, brightness, temperature, std::move(completed));
}

void SessionOutputManager::publishConfiguration()
{
    if (!configurationReady(m_config))
        return;
    QList<WOutputState> states;
    QSet<WOutput *> seen;
    for (auto *output : m_hardware->outputs()) {
        auto *physical = output->output();
        if (seen.contains(physical))
            continue;
        seen.insert(physical);
        auto *native = physical->handle();
        const auto position = physical->position();
        states.append(WOutputState {
            .output = physical,
            .enabled = physical->isEnabled(),
            .mode = native->current_mode,
            .x = position.x(),
            .y = position.y(),
            .customModeSize = QSize(native->width, native->height),
            .customModeRefresh = native->refresh,
            .transform = physical->orientation(),
            .scale = physical->scale(),
            .adaptiveSyncEnabled = native->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED
        });
        outputState(output)->arrangeLayerSurfaces();
    }
    m_protocol->setOutputStates(states);
    Q_EMIT desktopGeometryChanged();
}

void SessionOutputManager::requestConfiguration(wlr_output_configuration_v1 *configuration, bool onlyTest)
{
    m_channel->configure(m_protocol, configuration, onlyTest);
}

SessionOutput *SessionOutputManager::outputState(Output *output)
{
    if (auto *state = m_outputs.value(output))
        return state;
    auto *state = new SessionOutput(this, output);
    m_outputs.insert(output, state);
    connect(state, &SessionOutput::validGeometryChanged,
            this, &SessionOutputManager::desktopGeometryChanged);
    connect(output, &QObject::destroyed, this, [this, output] {
        delete m_outputs.take(output);
    });
    return state;
}

SessionOutput::SessionOutput(SessionOutputManager *manager, Output *output)
    : QObject(manager)
    , m_windows(new SurfaceListModel(this))
    , m_minimized(new SurfaceFilterModel(m_windows))
    , m_output(output)
    , m_surfaces(manager->surfaceRegistry())
{
    m_minimized->setFilter([](SurfaceWrapper *surface) { return surface->isMinimized(); });
    m_exclusiveZone = output->compositorExclusiveZone();
    m_lastValidGeometry = validGeometry();
}

QMargins SessionOutput::exclusiveZone() const
{
    return m_exclusiveZone;
}

QRectF SessionOutput::geometry() const
{
    return m_output ? m_output->geometry() : QRectF();
}

QRectF SessionOutput::validGeometry() const
{
    return geometry().marginsRemoved(m_exclusiveZone);
}

QList<SurfaceWrapper *> SessionOutput::layerSurfaces() const
{
    QList<SurfaceWrapper *> layers;
    for (const auto &layer : m_layers) {
        if (layer)
            layers.append(layer.data());
    }
    return layers;
}

SurfaceListModel *SessionOutput::windows() const
{
    return m_windows;
}

SurfaceListModel *SessionOutput::minimizedSurfaces() const
{
    return m_minimized;
}

void SessionOutput::addSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(m_surfaces->hasObject(surface));
    if (!m_windows->hasSurface(surface))
        m_windows->addSurface(surface);
    if (surface->type() == SurfaceWrapper::Type::Layer && !m_layers.contains(surface))
        m_layers.append(surface);
}

void SessionOutput::removeSurface(SurfaceWrapper *surface)
{
    m_layers.removeAll(surface);
    if (m_windows->hasSurface(surface))
        m_windows->removeSurface(surface);
}

void SessionOutput::addExclusiveZone(Qt::Edge edge, int value)
{
    Q_ASSERT(value > 0);
    switch (edge) {
    case Qt::TopEdge:
        m_exclusiveZone.setTop(m_exclusiveZone.top() + value);
        break;
    case Qt::BottomEdge:
        m_exclusiveZone.setBottom(m_exclusiveZone.bottom() + value);
        break;
    case Qt::LeftEdge:
        m_exclusiveZone.setLeft(m_exclusiveZone.left() + value);
        break;
    case Qt::RightEdge:
        m_exclusiveZone.setRight(m_exclusiveZone.right() + value);
        break;
    default:
        Q_UNREACHABLE_RETURN();
    }
}

void SessionOutput::arrangeLayerSurface(SurfaceWrapper *surface)
{
    WLayerSurface *layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
    Q_ASSERT(layer);
    if (!layer->handle()->initialized) {
        return;
    }

    auto validGeo = layer->exclusiveZone() == -1 ? geometry() : validGeometry();
    validGeo = validGeo.marginsRemoved(QMargins(layer->leftMargin(),
                                                layer->topMargin(),
                                                layer->rightMargin(),
                                                layer->bottomMargin()));
    auto anchor = layer->ancher();
    QRectF surfaceGeo(QPointF(0, 0), layer->desiredSize());

    if (anchor.testFlags(WLayerSurface::AnchorType::Left | WLayerSurface::AnchorType::Right)) {
        surfaceGeo.moveLeft(validGeo.left());
        surfaceGeo.setWidth(validGeo.width());
    } else if (anchor & WLayerSurface::AnchorType::Left) {
        surfaceGeo.moveLeft(validGeo.left());
    } else if (anchor & WLayerSurface::AnchorType::Right) {
        surfaceGeo.moveRight(validGeo.right());
    } else {
        surfaceGeo.moveLeft(validGeo.left() + (validGeo.width() - surfaceGeo.width()) / 2);
    }

    if (anchor.testFlags(WLayerSurface::AnchorType::Top | WLayerSurface::AnchorType::Bottom)) {
        surfaceGeo.moveTop(validGeo.top());
        surfaceGeo.setHeight(validGeo.height());
    } else if (anchor & WLayerSurface::AnchorType::Top) {
        surfaceGeo.moveTop(validGeo.top());
    } else if (anchor & WLayerSurface::AnchorType::Bottom) {
        surfaceGeo.moveBottom(validGeo.bottom());
    } else {
        surfaceGeo.moveTop(validGeo.top() + (validGeo.height() - surfaceGeo.height()) / 2);
    }

    if (layer->exclusiveZone() > 0) {
        // TODO: support set_exclusive_edge in layer-shell v5/wlroots 0.19
        switch (layer->getExclusiveZoneEdge()) {
            using enum WLayerSurface::AnchorType;
        case Top:
            addExclusiveZone(Qt::TopEdge, layer->exclusiveZone());
            break;
        case Bottom:
            addExclusiveZone(Qt::BottomEdge, layer->exclusiveZone());
            break;
        case Left:
            addExclusiveZone(Qt::LeftEdge, layer->exclusiveZone());
            break;
        case Right:
            addExclusiveZone(Qt::RightEdge, layer->exclusiveZone());
            break;
        default:
            qCWarning(lcTlOutput) << layer->appId()
                                 << " has set exclusive zone, but exclusive edge is invalid!";
            break;
        }
    }

    surface->setSize(surfaceGeo.size());
    surface->setPosition(surfaceGeo.topLeft());
}

void SessionOutput::arrangeLayerSurfaces()
{
    if (!m_output)
        return;
    const auto previous = m_exclusiveZone;
    m_exclusiveZone = m_output->compositorExclusiveZone();
    const auto layers = layerSurfaces();
    for (auto *surface : layers) {
        auto *layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
        if (layer && layer->exclusiveZone() > 0)
            arrangeLayerSurface(surface);
    }
    for (auto *surface : layers) {
        auto *layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
        if (layer && layer->exclusiveZone() <= 0)
            arrangeLayerSurface(surface);
    }
    if (previous != m_exclusiveZone)
        Q_EMIT exclusiveZoneChanged();
    const auto available = validGeometry();
    if (m_lastValidGeometry != available) {
        m_lastValidGeometry = available;
        Q_EMIT validGeometryChanged();
    }
}

DesktopSurfaceModel *SessionOutputManager::surfaceRegistry() const { return m_surfaces; }
bool SessionOutputManager::hardwareAttached() const { return m_channel->isConnected(); }
