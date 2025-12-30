// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSurface>
#include <wtoplevelsurface.h>
#include <WOutput>

struct wlr_layer_surface_v1;

QW_BEGIN_NAMESPACE
class qw_surface;
class qw_layer_surface_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WLayerSurfacePrivate;
class WAYLIB_SERVER_EXPORT WLayerSurface : public WToplevelSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WLayerSurface)
    Q_PROPERTY(WSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QSize desiredSize READ desiredSize NOTIFY desiredSizeChanged)

    Q_PROPERTY(LayerType layer READ layer NOTIFY layerChanged)
    Q_PROPERTY(AnchorTypes ancher READ ancher NOTIFY ancherChanged)
    Q_PROPERTY(int32_t exclusiveZone READ exclusiveZone NOTIFY exclusiveZoneChanged)
    Q_PROPERTY(int32_t leftMargin READ leftMargin NOTIFY leftMarginChanged)
    Q_PROPERTY(int32_t rightMargin READ rightMargin NOTIFY rightMarginChanged)
    Q_PROPERTY(int32_t topMargin READ topMargin NOTIFY topMarginChanged)
    Q_PROPERTY(int32_t bottomMargin READ bottomMargin NOTIFY bottomMarginChanged)
    Q_PROPERTY(KeyboardInteractivity keyboardInteractivity READ keyboardInteractivity NOTIFY keyboardInteractivityChanged)
    Q_PROPERTY(WOutput *output READ output CONSTANT) // constant in wlr_layershell_v1

    QML_NAMED_ELEMENT(WaylandLayerSurface)
    QML_UNCREATABLE("Only create in C++")
    // Name clash warning for enum class with the same named mebers
    // https://bugreports.qt.io/browse/QTBUG-73394
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

public:
    explicit WLayerSurface(QW_NAMESPACE::qw_layer_surface_v1 *handle, QObject *parent = nullptr);
    ~WLayerSurface();

    enum class LayerType {
        Background = 0,
        Bottom = 1,
        Top = 2,
        Overlay = 3,
    };
    Q_ENUM(LayerType)

    enum class AnchorType {
        None = 0,
        Top = 1,
        Bottom = 2,
        Left = 4,
        Right = 8,
    };
    Q_ENUM(AnchorType)
    Q_DECLARE_FLAGS(AnchorTypes, AnchorType)

    enum class KeyboardInteractivity {
        None = 0,
        Exclusive = 1,
        OnDemand = 2,
    };
    Q_ENUM(KeyboardInteractivity)

    bool hasCapability(Capability cap) const override;
    WSurface *surface() const override;
    QW_NAMESPACE::qw_layer_surface_v1 *handle() const;
    wlr_layer_surface_v1 *nativeHandle() const;
    QW_NAMESPACE::qw_surface *inputTargetAt(QPointF &localPos) const;

    static WLayerSurface *fromHandle(QW_NAMESPACE::qw_layer_surface_v1 *handle);
    static WLayerSurface *fromSurface(WSurface *surface);

    QRect getContentGeometry() const override;
    int keyboardFocusPriority() const override;
    bool isInitialized() const override;
    void resize(const QSize &size) override;
    void close() override;

    // layer shell info
    QSize desiredSize() const;
    LayerType layer() const;
    AnchorTypes ancher() const;
    int32_t exclusiveZone() const;
    int32_t leftMargin() const;
    int32_t rightMargin() const;
    int32_t topMargin() const;
    int32_t bottomMargin() const;
    KeyboardInteractivity keyboardInteractivity() const;
    QString scope() const;
    WOutput *output() const;
    Q_INVOKABLE AnchorType getExclusiveZoneEdge() const;
    Q_INVOKABLE uint32_t configureSize(const QSize &newSize);
    Q_INVOKABLE void closed();

    void updateLayerProperty();

Q_SIGNALS:
    void desiredSizeChanged();
    void surfaceChanged();
    void layerChanged();
    void ancherChanged();
    void leftMarginChanged();
    void rightMarginChanged();
    void topMarginChanged();
    void bottomMarginChanged();
    void exclusiveZoneChanged();
    void keyboardInteractivityChanged();
    void layerPropertiesChanged();

public Q_SLOTS:
    bool checkNewSize(const QSize &size,  QSize *clipedSize = nullptr) override;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WLayerSurface::AnchorTypes)

WAYLIB_SERVER_END_NAMESPACE
