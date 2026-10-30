// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QPointer>
#include <QSGRenderNode>
#include <QSGImageNode>
#include <QSGTextureProvider>
#include <QVector4D>
#include <memory>

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT RadiusTextureNode : public QObject, public QSGRenderNode
{
    Q_OBJECT

public:
    using TextureCoordinatesTransformMode = QSGImageNode::TextureCoordinatesTransformMode;

    explicit RadiusTextureNode(QQuickWindow *window);
    ~RadiusTextureNode() override;

    void setTexture(QSGTexture *texture);
    QSGTexture *texture() const;
    void setOwnsTexture(bool owns);
    bool ownsTexture() const;

    void setFiltering(QSGTexture::Filtering filtering);
    QSGTexture::Filtering filtering() const;
    void setMipmapFiltering(QSGTexture::Filtering filtering);
    QSGTexture::Filtering mipmapFiltering() const;
    void setAnisotropyLevel(QSGTexture::AnisotropyLevel level);
    QSGTexture::AnisotropyLevel anisotropyLevel() const;
    void setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode);
    TextureCoordinatesTransformMode textureCoordinatesTransform() const;

    void setTextureProvider(QSGTextureProvider *provider);
    void setRect(const QRectF &rect);
    void setViewport(const QRectF &viewport);
    void setSourceRect(const QRectF &rect);
    void setSourceOffset(const QPointF &offset);
    void setSourceSizeScale(qreal widthScale, qreal heightScale);
    void setRadius(qreal radius);
    void setTopLeftRadius(qreal radius);
    void setTopRightRadius(qreal radius);
    void setBottomLeftRadius(qreal radius);
    void setBottomRightRadius(qreal radius);
    void setAntialiasing(bool antialiasing);

    void preprocess() override;
    void prepare() override;
    void render(const RenderState *state) override;
    void releaseResources() override;
    QRectF rect() const override;
    RenderingFlags flags() const override;
    StateFlags changedStates() const override;

private Q_SLOTS:
    void handleTextureChanged();

private:
    void setTextureInternal(QSGTexture *texture);
    QRectF effectiveSourceRect() const;
    void updateGeometry();
    QVector4D effectiveRadii() const;
    void renderRhi(const RenderState *state);
    void renderSoftware(const RenderState *state);

    QPointer<QSGTextureProvider> m_provider;
    QSGTexture *m_texture = nullptr;
    struct RhiData;
    std::unique_ptr<RhiData> m_rhi;
    QPointer<QQuickWindow> m_window;
    struct Vertex {
        float x, y, textureX, textureY, localX, localY;
    };
    Vertex m_vertices[4] {};
    QSGTexture::Filtering m_filtering = QSGTexture::Nearest;
    QSGTexture::Filtering m_mipmapFiltering = QSGTexture::None;
    QSGTexture::AnisotropyLevel m_anisotropyLevel = QSGTexture::AnisotropyNone;
    QRectF m_rect;
    QRectF m_viewport;
    QPointF m_sourceOffset;
    QRectF m_sourceRect;
    QSize m_textureSize;
    TextureCoordinatesTransformMode m_texCoordMode = QSGImageNode::NoTransform;
    qreal m_sourceWidthScale = 1.0;
    qreal m_sourceHeightScale = 1.0;
    float m_radius = 0.0f;
    float m_topLeftRadius = -1.0f;
    float m_topRightRadius = -1.0f;
    float m_bottomLeftRadius = -1.0f;
    float m_bottomRightRadius = -1.0f;
    bool m_antialiasing = true;
    bool m_ownsTexture = false;
    bool m_geometryDirty = true;
};

WAYLIB_SERVER_END_NAMESPACE
