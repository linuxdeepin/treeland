// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutput.h>
#include <wtextureproviderprovider.h>
#include <qwglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewportPrivate;
class WSGTextureProvider;
class WOutputLayer;
class WAYLIB_SERVER_EXPORT WOutputViewport : public QQuickItem, public virtual WTextureProviderProvider
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WOutputViewport)
    Q_PROPERTY(QQuickItem* input READ input WRITE setInput NOTIFY inputChanged RESET resetInput FINAL)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput NOTIFY outputChanged REQUIRED)
    Q_PROPERTY(QQuickTransform* viewportTransform READ viewportTransform WRITE setViewportTransform NOTIFY viewportTransformChanged FINAL)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(bool offscreen READ offscreen WRITE setOffscreen NOTIFY offscreenChanged)
    Q_PROPERTY(bool cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged FINAL)
    Q_PROPERTY(bool preserveColorContents READ preserveColorContents WRITE setPreserveColorContents NOTIFY preserveColorContentsChanged FINAL)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged FINAL)
    Q_PROPERTY(QRectF sourceRect READ sourceRect WRITE setSourceRect RESET resetSourceRect NOTIFY sourceRectChanged FINAL)
    Q_PROPERTY(QRectF targetRect READ targetRect WRITE setTargetRect RESET resetTargetRect NOTIFY targetRectChanged FINAL)
    Q_PROPERTY(bool ignoreViewport READ ignoreViewport WRITE setIgnoreViewport NOTIFY ignoreViewportChanged FINAL)
    Q_PROPERTY(bool disableHardwareLayers READ disableHardwareLayers WRITE setDisableHardwareLayers NOTIFY disableHardwareLayersChanged FINAL)
    Q_PROPERTY(bool ignoreSoftwareLayers READ ignoreSoftwareLayers WRITE setIgnoreSoftwareLayers NOTIFY ignoreSoftwareLayersChanged FINAL)
    Q_PROPERTY(QList<WAYLIB_SERVER_NAMESPACE::WOutputLayer*> layers READ layers NOTIFY layersChanged FINAL)
    Q_PROPERTY(QList<WAYLIB_SERVER_NAMESPACE::WOutputLayer*> hardwareLayers READ hardwareLayers NOTIFY hardwareLayersChanged FINAL)
    Q_PROPERTY(QList<WAYLIB_SERVER_NAMESPACE::WOutputViewport*> depends READ depends WRITE setDepends NOTIFY dependsChanged FINAL)
    QML_NAMED_ELEMENT(OutputViewport)

public:
    explicit WOutputViewport(QQuickItem *parent = nullptr);
    ~WOutputViewport();

    Q_INVOKABLE void invalidate();

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    WSGTextureProvider *wTextureProvider() const override;
    WOutputRenderWindow *outputRenderWindow() const override;

    QQuickItem *input() const;
    void setInput(QQuickItem *item);
    void resetInput();

    WOutput *output() const;
    void setOutput(WOutput *newOutput);

    QQuickTransform *viewportTransform() const;
    void setViewportTransform(QQuickTransform *newViewportTransform);

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

    QRectF effectiveSourceRect() const;
    QRectF sourceRect() const;
    void setSourceRect(const QRectF &newSourceRect);
    void resetSourceRect();

    QRectF targetRect() const;
    void setTargetRect(const QRectF &newTargetRect);
    void resetTargetRect();

    QTransform sourceRectToTargetRectTransfrom() const;
    QMatrix4x4 renderMatrix() const;
    QMatrix4x4 mapToViewport(QQuickItem *item) const;
    Q_INVOKABLE QRectF mapToOutput(QQuickItem *source, const QRectF &geometry) const;
    Q_INVOKABLE QPointF mapToOutput(QQuickItem *source, const QPointF &position) const;

    bool ignoreViewport() const;
    void setIgnoreViewport(bool newIgnoreViewport);

    bool disableHardwareLayers() const;
    void setDisableHardwareLayers(bool newDisableHardwareLayers);

    bool ignoreSoftwareLayers() const;
    void setIgnoreSoftwareLayers(bool newIgnoreSoftwareLayers);

    QList<WOutputLayer*> layers() const;
    QList<WOutputLayer*> hardwareLayers() const;

    QList<WOutputViewport *> depends() const;
    void setDepends(const QList<WOutputViewport *> &newDepends);

public Q_SLOTS:
    void setOutputScale(float scale);
    void rotateOutput(WOutput::Transform t);
    void render(bool doCommit);

Q_SIGNALS:
    void devicePixelRatioChanged();
    void offscreenChanged();
    void cacheBufferChanged();
    void preserveColorContentsChanged();
    void outputRenderInitialized();
    void inputChanged();
    void liveChanged();
    void outputChanged();
    void viewportTransformChanged();
    void sourceRectChanged();
    void targetRectChanged();
    void ignoreViewportChanged();
    void disableHardwareLayersChanged();
    void ignoreSoftwareLayersChanged();
    void layersChanged();
    void hardwareLayersChanged();
    void dependsChanged();

private:
    void componentComplete() override;
    void releaseResources() override;
    void itemChange(ItemChange, const ItemChangeData &) override;
};

WAYLIB_SERVER_END_NAMESPACE
