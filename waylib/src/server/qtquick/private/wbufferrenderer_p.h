// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wscopedvalue.h>
#include <wlr_fwd.h>
#include <wglobal.h>
#include <wpointer.h>
#include <woutputrenderwindow.h>
#include <wrenderhelper.h>

#include <wlr_all.h>

#include <QQuickItem>
#include <QQuickRenderTarget>
#include <private/qsgrenderer_p.h>

Q_MOC_INCLUDE(<private/qsgplaintexture_p.h>)

QT_BEGIN_NAMESPACE
class QSGPlainTexture;
class QSGRenderContext;
namespace WSGBatchRenderer {
class Renderer;
}
QT_END_NAMESPACE

struct pixman_region32;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGTextureProvider;
class WAYLIB_SERVER_EXPORT WBufferRenderer : public QQuickItem
{
    friend class WOutputRenderWindow;
    friend class WOutputRenderWindowPrivate;
    friend class OutputHelper;
    Q_OBJECT

public:
    enum RenderFlag {
        DontConfigureSwapchain = 1,
        DontTestSwapchain = 2,
        RedirectOpenGLContextDefaultFrameBufferObject = 4,
        UseCursorFormats = 8,
    };
    Q_DECLARE_FLAGS(RenderFlags, RenderFlag)

    explicit WBufferRenderer(QQuickItem *parent = nullptr);
    ~WBufferRenderer();

    WOutput *output() const;
    void setOutput(WOutput *output);

    int sourceCount() const;
    QList<QQuickItem*> sourceList() const;
    void setSourceList(QList<QQuickItem *> sources, bool hideSource);

    bool cacheBuffer() const;
    void setCacheBuffer(bool newCacheBuffer);

    void lockCacheBuffer(QObject *owner);
    void unlockCacheBuffer(QObject *owner);

    QColor clearColor() const;
    void setClearColor(const QColor &clearColor);

    QSGRenderer *currentRenderer() const;
    WSGBatchRenderer::Renderer *currentBatchRenderer() const;
    qreal currentDevicePixelRatio() const;
    const QMatrix4x4 &currentWorldTransform() const;
    wlr_buffer *currentBuffer() const;
    wlr_buffer *lastBuffer() const;
    QRhiTexture *currentRenderTarget() const;
    bool isColorPreserved() const;
    const wlr_damage_ring *damageRing() const;
    wlr_damage_ring *damageRing();

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;
    WSGTextureProvider *wTextureProvider() const;

    static QTransform inputMapToOutput(const QRectF &sourceRect, const QRectF &targetRect,
                                       const QSize &pixelSize, const qreal devicePixelRatio);

Q_SIGNALS:
    void sceneGraphChanged();
    void devicePixelRatioChanged();
    void cacheBufferChanged();
    void beforeRendering();
    void afterRendering();

protected:
    wlr_buffer *beginRender(const QSize &pixelSize, qreal devicePixelRatio,
                            uint32_t format, RenderFlags flags = {},
                            WGlobal::ColorContentsMode mode = WGlobal::ColorContentsMode::DontCare);
    void render(int sourceIndex, const QMatrix4x4 &renderMatrix,
                const QRectF &sourceRect = {}, const QRectF &targetRect = {});
    void endRender();
    void componentComplete() override;

private:
    inline WOutputRenderWindow *renderWindow() const {
        return qobject_cast<WOutputRenderWindow*>(window());
    }

    inline bool shouldCacheBuffer() const {
        return m_cacheBuffer || !m_cacheBufferLocker.isEmpty();
    }

    void updateTextureProvider();
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    Q_SLOT void invalidateSceneGraph();
    void releaseResources() override;
    void cleanTextureProvider();

    inline bool isRootItem(const QQuickItem *source) const {
        return nullptr == source;
    }

    void resetSources();
    void destroySource(int index);
    int indexOfSource(QQuickItem *item);
    QSGRenderer *ensureRenderer(int sourceIndex, QSGRenderContext *rc);

    WUniquePointer<wlr_swapchain> m_swapchain;
    WRenderHelper *m_renderHelper = nullptr;
    WPointer<wlr_buffer> m_lastBuffer;

    struct RenderState {
        RenderFlags flags;
        WGlobal::ColorContentsMode colorContentsMode = WGlobal::ColorContentsMode::DontCare;
        QSGRenderContext *context;
        QSGRenderer *renderer;
        WSGBatchRenderer::Renderer *batchRenderer;
        QMatrix4x4 worldTransform;
        QSize pixelSize;
        qreal devicePixelRatio;
        WBufferUnlockPtr buffer;
        WRenderHelper::RenderTarget renderTarget;
        QSGRenderTarget sgRenderTarget;
        QRegion dirty;
    } state;

    QPointer<WOutput> m_output;

    struct Data {
        QQuickItem *source = nullptr; // Don't using QPointer, See isRootItem
        QSGRenderer *renderer = nullptr;
    };

    QList<Data> m_sourceList;
    WDamageRing m_damageRing;
    mutable std::unique_ptr<WSGTextureProvider> m_textureProvider;
    QColor m_clearColor = Qt::transparent;
    QList<QObject*> m_cacheBufferLocker;

    uint m_cacheBuffer:1;
    uint m_hideSource:1;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WBufferRenderer::RenderFlags)
