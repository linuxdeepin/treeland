// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "output/output.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QHash>
#include <QList>
#include <QMargins>
#include <QRectF>
#include <QString>
#include <QPointer>
#include <woutputmanagerv1.h>
#include <functional>

class Output;
class OutputConfig;
class TreelandConfig;
class DesktopSurfaceModel;
class SessionOutputChannel;
class OutputManager;
class SurfaceWrapper;
class SurfaceListModel;
class SurfaceFilterModel;
class SessionOutputManager;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
WAYLIB_SERVER_END_NAMESPACE

Q_MOC_INCLUDE("output/output.h")
Q_MOC_INCLUDE(<woutput.h>)

// Logical output owned by one user; it never includes another user's panels.
class SessionOutput : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(SurfaceListModel *windows READ windows CONSTANT FINAL)
    Q_PROPERTY(SurfaceListModel *minimizedSurfaces READ minimizedSurfaces CONSTANT FINAL)
    Q_PROPERTY(QMargins exclusiveZone READ exclusiveZone NOTIFY exclusiveZoneChanged FINAL)
    Q_PROPERTY(QRectF validGeometry READ validGeometry NOTIFY validGeometryChanged FINAL)
public:
    SessionOutput(SessionOutputManager *manager, Output *output);
    QMargins exclusiveZone() const;
    QRectF geometry() const;
    QRectF validGeometry() const;
    QList<SurfaceWrapper *> layerSurfaces() const;
    SurfaceListModel *windows() const;
    SurfaceListModel *minimizedSurfaces() const;
    void addSurface(SurfaceWrapper *surface);
    void removeSurface(SurfaceWrapper *surface);
    void arrangeLayerSurfaces();

Q_SIGNALS:
    void exclusiveZoneChanged();
    void validGeometryChanged();

private:
    void arrangeLayerSurface(SurfaceWrapper *surface);
    void addExclusiveZone(Qt::Edge edge, int value);
    SurfaceListModel *m_windows;
    SurfaceFilterModel *m_minimized;
    QPointer<Output> m_output;
    DesktopSurfaceModel *const m_surfaces;
    QList<QPointer<SurfaceWrapper>> m_layers;
    QMargins m_exclusiveZone;
    QRectF m_lastValidGeometry;
};

// Per-Session protocol and window layout; display settings are system-wide.
class SessionOutputManager : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    using ConfigProvider = std::function<OutputConfig *(const QString &)>;
    SessionOutputManager(TreelandConfig *config, ConfigProvider outputConfiguration,
                         DesktopSurfaceModel *surfaces, SessionOutputChannel *channel,
                         OutputManager *hardware, WAYLIB_SERVER_NAMESPACE::WBackend *backend,
                         WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *protocol,
                         QObject *parent);
    DesktopSurfaceModel *surfaceRegistry() const;
    bool hardwareAttached() const;
    OutputConfig *config(const QString &outputId);
    SessionOutput *outputState(Output *output);
    WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *protocol() const { return m_protocol; }
    void refresh();
    Q_INVOKABLE void setScale(WAYLIB_SERVER_NAMESPACE::WOutput *output, qreal scale);
    qreal effectiveScale() const;
    qreal scaleAt(const QPointF &position);
    QPointF defaultPosition(Output *output);
    QString primaryOutputName() const;
    void setPrimaryOutput(const QString &name);
    void applyColor(Output *output, qreal brightness, uint32_t temperature,
                    std::function<void(Output::CommitColorResult)> completed);

Q_SIGNALS:
    void desktopGeometryChanged();

private:
    void requestConfiguration(wlr_output_configuration_v1 *configuration, bool onlyTest);
    void publishConfiguration();
    TreelandConfig *const m_config;
    const ConfigProvider m_outputConfiguration;
    DesktopSurfaceModel *const m_surfaces;
    SessionOutputChannel *const m_channel;
    OutputManager *const m_hardware;
    WAYLIB_SERVER_NAMESPACE::WOutputManagerV1 *m_protocol;
    QHash<QString, OutputConfig *> m_configs;
    QHash<Output *, SessionOutput *> m_outputs;
};
