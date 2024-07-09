// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutput.h>
#include <qwglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewportPrivate;
class WBufferTextureProvider;
class WAYLIB_SERVER_EXPORT WOutputViewport : public QQuickItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputViewport)
    Q_PROPERTY(QQuickItem* input READ input WRITE setInput NOTIFY inputChanged RESET resetInput FINAL)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput NOTIFY outputChanged REQUIRED)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(bool offscreen READ offscreen WRITE setOffscreen NOTIFY offscreenChanged)
    Q_PROPERTY(bool cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged FINAL)
    Q_PROPERTY(bool preserveColorContents READ preserveColorContents WRITE setPreserveColorContents NOTIFY preserveColorContentsChanged FINAL)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged FINAL)
    Q_PROPERTY(LayerFlags layerFlags READ layerFlags WRITE setLayerFlags NOTIFY layerFlagsChanged FINAL)
    QML_NAMED_ELEMENT(OutputViewport)

public:
    enum class LayerFlag {
        AlwaysAccepted = 1,
        AlwaysRejected = 2,
        UsingShadowBufferOnComposite = 4,
    };
    Q_ENUM(LayerFlag)
    Q_DECLARE_FLAGS(LayerFlags, LayerFlag)

    explicit WOutputViewport(QQuickItem *parent = nullptr);
    ~WOutputViewport();

    Q_INVOKABLE void invalidate();

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    WBufferTextureProvider *wTextureProvider() const;

    QQuickItem *input() const;
    void setInput(QQuickItem *item);
    void resetInput();

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal newDevicePixelRatio);

    bool offscreen() const;
    void setOffscreen(bool newOffscreen);

    bool cacheBuffer() const;
    void setCacheBuffer(bool newCacheBuffer);

    bool preserveColorContents() const;
    void setPreserveColorContents(bool newPreserveColorContents);

    bool live() const;
    void setLive(bool newLive);

    LayerFlags layerFlags() const;
    void setLayerFlags(const LayerFlags &newLayerFlags);

public Q_SLOTS:
    void setOutputScale(float scale);
    void rotateOutput(WOutput::Transform t);
    void render(bool doCommit);

Q_SIGNALS:
    void devicePixelRatioChanged();
    void offscreenChanged();
    void cacheBufferChanged();
    void preserveColorContentsChanged();
    void layerFlagsChanged();
    void outputRenderInitialized();
    void inputChanged();
    void liveChanged();
    void outputChanged();

private:
    void componentComplete() override;
    void releaseResources() override;
    void itemChange(ItemChange, const ItemChangeData &) override;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WOutputViewport::LayerFlags)
