// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wradiustexturematerial_p.h"

#include <QObject>
#include <QPointer>
#include <QSGGeometryNode>
#include <QSGImageNode>
#include <QSGTextureProvider>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT RadiusTextureNode : public QObject, public QSGGeometryNode
{
    Q_OBJECT

public:
    using TextureCoordinatesTransformMode = QSGImageNode::TextureCoordinatesTransformMode;

    RadiusTextureNode();
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

private Q_SLOTS:
    void handleTextureChanged();

private:
    void setTextureInternal(QSGTexture *texture);
    QRectF effectiveSourceRect() const;
    void updateGeometry();
    void updateMaterial();

    QPointer<QSGTextureProvider> m_provider;
    QSGTexture *m_texture = nullptr;
    QSGGeometry m_geometry;
    RadiusTextureMaterial m_material;
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
