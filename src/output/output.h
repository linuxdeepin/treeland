// Copyright (C) 2024-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surface/surfacecontainer.h"
#include "backlight.h"

#include <wglobal.h>
#include <woutputviewport.h>

#include <QMargins>
#include <QObject>
#include <QQmlComponent>

Q_MOC_INCLUDE(<woutputitem.h>)

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
class OutputConfig;

class Output : public SurfaceListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QMargins exclusiveZone READ exclusiveZone NOTIFY exclusiveZoneChanged FINAL)
    Q_PROPERTY(QRectF validRect READ validRect NOTIFY exclusiveZoneChanged FINAL)
    Q_PROPERTY(WOutputItem* outputItem MEMBER m_item CONSTANT)
    Q_PROPERTY(SurfaceListModel* minimizedSurfaces MEMBER minimizedSurfaces CONSTANT)
    Q_PROPERTY(WOutputViewport* screenViewport MEMBER m_outputViewport CONSTANT)
    Q_PROPERTY(OutputConfig* config READ config CONSTANT FINAL)

public:
    enum class Type
    {
        Primary,
        Proxy
    };

    enum class PlaceDirection
    {
        TopLeft,
        BottomRight,
    };

    static Output *create(WOutput *output, QQmlEngine *engine, QObject *parent = nullptr);
    static Output *createCopy(WOutput *output,
                              Output *proxy,
                              QQmlEngine *engine,
                              QObject *parent = nullptr);

    explicit Output(WOutputItem *output, QObject *parent = nullptr);
    ~Output() override;

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
    QQuickItem *debugMenuBar() const;

    static double calcPreferredScale(double widthPx,
                                     double heightPx,
                                     double widthMm,
                                     double heightMm);
    qreal preferredScaleFactor(const QSize &pixelSize) const;

    OutputConfig* config() const;

Q_SIGNALS:
    void exclusiveZoneChanged();
    void moveResizeFinised();
    void brightnessChanged();
    void colorTemperatureChanged();

public Q_SLOTS:
    void enable();
    void updateOutputHardwareLayers();
    void setOutputColor(qreal brightness,
                        uint32_t colorTemperature,
                        std::function<void(bool)> resultCallback = nullptr);

private:
    friend class SurfaceWrapper;

    void setExclusiveZone(Qt::Edge edge, QObject *object, int value);
    bool removeExclusiveZone(QObject *object);
    void arrangeLayerSurface(SurfaceWrapper *surface);
    void arrangeLayerSurfaces();
    void arrangeNonLayerSurface(SurfaceWrapper *surface, const QSizeF &sizeDiff);
    void arrangePopupSurface(SurfaceWrapper *surface);
    void arrangeNonLayerSurfaces();
    void arrangeAllSurfaces();
    std::pair<WOutputViewport *, QQuickItem *> getOutputItemProperty();
    void placeUnderCursor(SurfaceWrapper *surface, quint32 yOffset);
    void placeClientRequstPos(SurfaceWrapper *surface, QPoint clientRequstPos);
    void placeCentered(SurfaceWrapper *surface);
    void placeSmartCascaded(SurfaceWrapper *surface);
    QPointF calculateBottomRightPosition(const QRectF &activeGeo,
                                         const QRectF &normalGeo,
                                         const QRectF &validGeo,
                                         const QSizeF &offset);
    QPointF calculateTopLeftPosition(const QRectF &activeGeo,
                                     const QRectF &normalGeo,
                                     const QRectF &validGeo,
                                     const QSizeF &offset);
    QPointF constrainToValidArea(const QPointF &pos,
                                 const QSizeF &windowSize,
                                 const QRectF &validGeo);
    qreal preferredScaleFactor() const;

    QPointF calculateBasePosition(SurfaceWrapper *surface, const QPointF &dPos) const;
    void adjustToOutputBounds(QPointF &pos, const QRectF &normalGeo, const QRectF &outputRect) const;
    void handleLayerShellPopup(SurfaceWrapper *surface, const QRectF &normalGeo);
    void handleRegularPopup(SurfaceWrapper *surface, const QRectF &normalGeo, bool isSubMenu, WOutputItem *targetOutput);
    void clearPopupCache(SurfaceWrapper *surface);

    Type m_type;
    WOutputItem *m_item;
    Output *m_proxy = nullptr;
    SurfaceFilterModel *minimizedSurfaces;
    QPointer<QQuickItem> m_taskBar;
    QPointer<QQuickItem> m_debugMenuBar;
    WOutputViewport *m_outputViewport = nullptr;

    QMargins m_exclusiveZone;
    QList<std::pair<QObject *, int>> m_topExclusiveZones;
    QList<std::pair<QObject *, int>> m_bottomExclusiveZones;
    QList<std::pair<QObject *, int>> m_leftExclusiveZones;
    QList<std::pair<QObject *, int>> m_rightExclusiveZones;

    QSizeF m_lastSizeOnLayoutNonLayerSurfaces;
    QList<WOutputLayer *> m_hardwareLayersOfPrimaryOutput;
    PlaceDirection m_nextPlaceDirection = PlaceDirection::BottomRight;

    QMap<SurfaceWrapper*, QPair<QPointF, QRectF>> m_positionCache;
    QHash<SurfaceWrapper*, QPointF> m_initialWindowPositionRatio;

    std::unique_ptr<Backlight> m_backlight = nullptr;
    OutputConfig *m_config;
};

Q_DECLARE_OPAQUE_POINTER(WAYLIB_SERVER_NAMESPACE::WOutputItem *)
