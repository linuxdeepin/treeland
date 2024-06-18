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
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWBuffer;
class QWTexture;
class QWSwapchain;
QW_END_NAMESPACE

struct pixman_region32;
struct wlr_swapchain;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WRenderHelper;
class TextureProvider;
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

    QSGRenderer *currentRenderer() const;
    const QMatrix4x4 &currentWorldTransform() const;
    QW_NAMESPACE::QWBuffer *currentBuffer() const;
    QW_NAMESPACE::QWBuffer *lastBuffer() const;
    QRhiTexture *currentRenderTarget() const;
    const QW_NAMESPACE::QWDamageRing *damageRing() const;
    QW_NAMESPACE::QWDamageRing *damageRing();

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

Q_SIGNALS:
    void sceneGraphChanged();
    void devicePixelRatioChanged();
    void cacheBufferChanged();
    void beforeRendering();
    void afterRendering();

protected:
    QW_NAMESPACE::QWBuffer *beginRender(const QSize &pixelSize, qreal devicePixelRatio,
                                        uint32_t format, RenderFlags flags = {});
    void render(int sourceIndex, const QMatrix4x4 &renderMatrix, bool preserveColorContents = false);
    void endRender();
    void componentComplete() override;

private:
    inline WOutputRenderWindow *renderWindow() const {
        return qobject_cast<WOutputRenderWindow*>(parent());
    }

    inline bool shouldCacheBuffer() const {
        return m_cacheBuffer || m_forceCacheBuffer;
    }

    void setForceCacheBuffer(bool force);
    void resetTextureProvider();
    void updateTextureProvider();
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    Q_SLOT void invalidateSceneGraph();
    void releaseResources() override;

    inline bool isRootItem(const QQuickItem *source) const {
        return nullptr == source;
    }

    void resetSources();
    void removeSource(int index);
    int indexOfSource(QQuickItem *item);
    QSGRenderer *ensureRenderer(int sourceIndex, QSGRenderContext *rc);

    QW_NAMESPACE::QWSwapchain *m_swapchain = nullptr;
    WRenderHelper *m_renderHelper = nullptr;
    QPointer<QW_NAMESPACE::QWBuffer> m_lastBuffer;

    struct RenderState {
        RenderFlags flags;
        QSGRenderContext *context;
        QSGRenderer *renderer;
        QMatrix4x4 worldTransform;
        QSize pixelSize;
        qreal devicePixelRatio;
        int bufferAge;
        std::pair<QW_NAMESPACE::QWBuffer*, QQuickRenderTarget> lastRT;
        QW_NAMESPACE::QWBuffer *buffer = nullptr;
        QQuickRenderTarget renderTarget;
        QSGRenderTarget sgRenderTarget;
    } state;

    QPointer<WOutput> m_output;

    struct Data {
        QQuickItem *source = nullptr; // Don't using QPointer, See isRootItem
        QSGRenderer *renderer = nullptr;
    };

    QList<Data> m_sourceList;
    QW_NAMESPACE::QWDamageRing m_damageRing;
    std::unique_ptr<TextureProvider> m_textureProvider;

    uint m_cacheBuffer:1;
    uint m_forceCacheBuffer:1;
    uint m_hideSource:1;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WBufferRenderer::RenderFlags)
