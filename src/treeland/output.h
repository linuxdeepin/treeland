// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacecontainer.h"

#include <wglobal.h>
#include <woutputviewport.h>

#include <QMargins>
#include <QObject>
#include <QQmlComponent>

Q_MOC_INCLUDE(<woutputitem.h>)

Q_DECLARE_LOGGING_CATEGORY(qLcLayerShell)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
class WOutputItem;
class WOutputViewport;
class WOutputLayout;
class WOutputLayer;
class WQuickTextureProxy;
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SurfaceWrapper;

class Output : public SurfaceListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QMargins exclusiveZone READ exclusiveZone NOTIFY exclusiveZoneChanged FINAL)
    Q_PROPERTY(QRectF validRect READ validRect NOTIFY exclusiveZoneChanged FINAL)
    Q_PROPERTY(WOutputItem* outputItem MEMBER m_item CONSTANT)
    Q_PROPERTY(SurfaceListModel* minimizedSurfaces MEMBER minimizedSurfaces CONSTANT)
    Q_PROPERTY(WOutputViewport* screenViewport MEMBER m_outputViewport CONSTANT)

public:
    enum class Type
    {
        Primary,
        Proxy
    };

    static Output *create(WOutput *output, QQmlEngine *engine, QObject *parent = nullptr);
    static Output *createCopy(WOutput *output,
                              Output *proxy,
                              QQmlEngine *engine,
                              QObject *parent = nullptr);

    explicit Output(WOutputItem *output, QObject *parent = nullptr);
    ~Output();

    bool isPrimary() const;

    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

    WOutput *output() const;
    WOutputItem *outputItem() const;

    QMargins exclusiveZone() const;
    QRectF rect() const;
    QRectF geometry() const;
    QRectF validRect() const;
    QRectF validGeometry() const;
    WOutputViewport *screenViewport() const;
    void updatePositionFromLayout();
#ifdef QT_DEBUG
    QQuickItem *outputMenuBar() const;
#endif

Q_SIGNALS:
    void exclusiveZoneChanged();
    void moveResizeFinised();

public Q_SLOTS:
    void updateOutputHardwareLayers();

private:
    friend class SurfaceWrapper;

    void setExclusiveZone(Qt::Edge edge, QObject *object, int value);
    bool removeExclusiveZone(QObject *object);
    void arrangeLayerSurface(SurfaceWrapper *surface);
    void arrangeLayerSurfaces();
    void arrangeNonLayerSurface(SurfaceWrapper *surface, const QSizeF &sizeDiff);
    void arrangeNonLayerSurfaces();
    void arrangeAllSurfaces();
    std::pair<WOutputViewport *, QQuickItem *> getOutputItemProperty();

    Type m_type;
    WOutputItem *m_item;
    Output *m_proxy = nullptr;
    SurfaceFilterModel *minimizedSurfaces;
    QPointer<QQuickItem> m_taskBar;
#ifdef QT_DEBUG
    QPointer<QQuickItem> m_menuBar;
#endif
    WOutputViewport *m_outputViewport = nullptr;

    QMargins m_exclusiveZone;
    QList<std::pair<QObject *, int>> m_topExclusiveZones;
    QList<std::pair<QObject *, int>> m_bottomExclusiveZones;
    QList<std::pair<QObject *, int>> m_leftExclusiveZones;
    QList<std::pair<QObject *, int>> m_rightExclusiveZones;

    QSizeF m_lastSizeOnLayoutNonLayerSurfaces;
    QList<WOutputLayer *> m_hardwareLayersOfPrimaryOutput;
};

Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WOutputItem *)
