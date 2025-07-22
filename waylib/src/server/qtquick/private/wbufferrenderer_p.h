// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwdamagering.h>
#include <wglobal.h>
#include <woutputrenderwindow.h>

#include <QQuickItem>
#include <QQuickRenderTarget>
#define protected public
#include <private/qsgrenderer_p.h>
#undef protected

Q_MOC_INCLUDE(<private/qsgplaintexture_p.h>)

QT_BEGIN_NAMESPACE
class QSGPlainTexture;
class QSGRenderContext;
namespace QSGBatchRenderer {
class Renderer;
}
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_buffer;
class qw_swapchain;
QW_END_NAMESPACE

struct pixman_region32;
struct wlr_swapchain;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderHelper;
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
    QSGBatchRenderer::Renderer *currentBatchRenderer() const;
    qreal currentDevicePixelRatio() const;
    const QMatrix4x4 &currentWorldTransform() const;
    QW_NAMESPACE::qw_buffer *currentBuffer() const;
    QW_NAMESPACE::qw_buffer *lastBuffer() const;
    QRhiTexture *currentRenderTarget() const;
    const QW_NAMESPACE::qw_damage_ring *damageRing() const;
    QW_NAMESPACE::qw_damage_ring *damageRing();

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
    QW_NAMESPACE::qw_buffer *beginRender(const QSize &pixelSize, qreal devicePixelRatio,
                                        uint32_t format, RenderFlags flags = {});
    void render(int sourceIndex, const QMatrix4x4 &renderMatrix,
                const QRectF &sourceRect = {}, const QRectF &targetRect = {},
                bool preserveColorContents = false);
    void endRender();
    void componentComplete() override;

private:
    inline WOutputRenderWindow *renderWindow() const {
        return qobject_cast<WOutputRenderWindow*>(parent());
    }

    inline bool shouldCacheBuffer() const {
        return m_cacheBuffer || !m_cacheBufferLocker.isEmpty();
    }

    void resetTextureProvider();
    void updateTextureProvider();
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    Q_SLOT void invalidateSceneGraph();
    void releaseResources() override;
    void cleanTextureProvider();

    inline bool isRootItem(const QQuickItem *source) const {
        return nullptr == source;
    }

    void resetSources();
    void removeSource(int index);
    int indexOfSource(QQuickItem *item);
    QSGRenderer *ensureRenderer(int sourceIndex, QSGRenderContext *rc);

    QW_NAMESPACE::qw_swapchain *m_swapchain = nullptr;
    WRenderHelper *m_renderHelper = nullptr;
    QPointer<QW_NAMESPACE::qw_buffer> m_lastBuffer;

    struct RenderState {
        RenderFlags flags;
        QSGRenderContext *context;
        QSGRenderer *renderer;
        QSGBatchRenderer::Renderer *batchRenderer;
        QMatrix4x4 worldTransform;
        QSize pixelSize;
        qreal devicePixelRatio;
        QW_NAMESPACE::qw_buffer *buffer = nullptr;
        QQuickRenderTarget renderTarget;
        QSGRenderTarget sgRenderTarget;
        QRegion dirty;
    } state;

    QPointer<WOutput> m_output;

    struct Data {
        QQuickItem *source = nullptr; // Don't using QPointer, See isRootItem
        QSGRenderer *renderer = nullptr;
    };

    QList<Data> m_sourceList;
    QW_NAMESPACE::qw_damage_ring m_damageRing;
    mutable std::unique_ptr<WSGTextureProvider> m_textureProvider;
    QColor m_clearColor = Qt::transparent;
    QList<QObject*> m_cacheBufferLocker;

    uint m_cacheBuffer:1;
    uint m_hideSource:1;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WBufferRenderer::RenderFlags)
