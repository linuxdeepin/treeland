// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <qwdamagering.h>
#include <wglobal.h>
#include <woutputrenderwindow.h>

#include <QQuickItem>
#include <QQuickRenderTarget>

Q_MOC_INCLUDE(<private/qsgplaintexture_p.h>)

QT_BEGIN_NAMESPACE
class QSGRenderer;
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
        UpdateResource,
    };
    Q_DECLARE_FLAGS(RenderFlags, RenderFlag)

    explicit WBufferRenderer(QQuickItem *parent = nullptr);
    ~WBufferRenderer();

    WOutput *output() const;
    void setOutput(WOutput *output);

    QQuickItem *source() const;
    void setSource(QQuickItem *s, bool hideSource);

    bool cacheBuffer() const;
    void setCacheBuffer(bool newCacheBuffer);

    QW_NAMESPACE::QWBuffer *lastBuffer() const;
    const QW_NAMESPACE::QWDamageRing *damageRing() const;
    QW_NAMESPACE::QWDamageRing *damageRing();

    bool isTextureProvider() const override;
    QSGTextureProvider *textureProvider() const override;

Q_SIGNALS:
    void sceneGraphChanged();
    void devicePixelRatioChanged();
    void cacheBufferChanged();

protected:
    QW_NAMESPACE::QWBuffer *render(QSGRenderContext *context, uint32_t format,
                                   const QSize &pixelSize, qreal devicePixelRatio,
                                   QMatrix4x4 renderMatrix, RenderFlags flags = {});
    void setBufferSubmitted(QW_NAMESPACE::QWBuffer *buffer);
    void componentComplete() override;

private:
    inline WOutputRenderWindow *renderWindow() const {
        return qobject_cast<WOutputRenderWindow*>(parent());
    }

    inline bool shouldCacheBuffer() const {
        return m_cacheBuffer || m_forceCacheBuffer;
    }

    void ensureTextureProvider();
    void resetTextureProvider();
    void updateTextureProvider();
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    Q_SLOT void invalidateSceneGraph();
    void releaseResources() override;
    void resetSource();

    QW_NAMESPACE::QWSwapchain *m_swapchain = nullptr;
    QPointer<QW_NAMESPACE::QWBuffer> m_lastBuffer;
    QSGRenderer *m_renderer = nullptr;
    WRenderHelper *m_renderHelper = nullptr;

    QPointer<WOutput> m_output;
    QQuickItem *m_source = nullptr;
    QSGRootNode *m_rootNode = nullptr;
    QW_NAMESPACE::QWDamageRing m_damageRing;
    std::unique_ptr<TextureProvider> m_textureProvider;

    uint m_cacheBuffer:1;
    uint m_forceCacheBuffer:1;
    uint m_hideSource:1;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WBufferRenderer::RenderFlags)
