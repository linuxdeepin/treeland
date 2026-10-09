// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wradiustexturenode_p.h"

#include <QSGDynamicTexture>
#include <QSGTexture>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
struct RadiusTextureVertex {
    float x;
    float y;
    float textureX;
    float textureY;
    float localX;
    float localY;
};

const QSGGeometry::AttributeSet &radiusTextureAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(
            0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(
            2, 2, QSGGeometry::FloatType, QSGGeometry::TexCoord1Attribute)
    };
    static const QSGGeometry::AttributeSet attributeSet = {
        3, sizeof(RadiusTextureVertex), attributes
    };
    return attributeSet;
}

struct CornerRadii {
    qreal topLeft;
    qreal topRight;
    qreal bottomRight;
    qreal bottomLeft;
};

CornerRadii normalizedRadii(const QSizeF &size, CornerRadii radii)
{
    const qreal maximumRadius = qMax<qreal>(0.0, qMin(size.width(), size.height()) * 0.5);
    return { qBound<qreal>(0.0, radii.topLeft, maximumRadius),
             qBound<qreal>(0.0, radii.topRight, maximumRadius),
             qBound<qreal>(0.0, radii.bottomRight, maximumRadius),
             qBound<qreal>(0.0, radii.bottomLeft, maximumRadius) };
}
}

RadiusTextureNode::RadiusTextureNode()
    : m_geometry(radiusTextureAttributes(), 0)
{
    setFlag(UsePreprocess, true);
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    m_geometry.setVertexDataPattern(QSGGeometry::DynamicPattern);
    setGeometry(&m_geometry);
    setMaterial(&m_material);
}

RadiusTextureNode::~RadiusTextureNode()
{
    m_material.setTexture(nullptr);
    if (m_ownsTexture)
        delete m_texture;
}

void RadiusTextureNode::setTexture(QSGTexture *texture)
{
    if (m_provider) {
        disconnect(m_provider, &QSGTextureProvider::textureChanged,
                   this, &RadiusTextureNode::handleTextureChanged);
        m_provider = nullptr;
    }
    setTextureInternal(texture);
}

void RadiusTextureNode::setTextureInternal(QSGTexture *texture)
{
    if (m_texture == texture) {
        // A provider may reuse the wrapper while replacing its native texture.
        if (!texture && m_geometry.vertexCount() != 0)
            m_geometry.allocate(0);
        m_geometryDirty = true;
        markDirty(DirtyGeometry | DirtyMaterial);
        return;
    }

    if (m_ownsTexture)
        delete m_texture;
    m_texture = texture;
    m_material.setTexture(texture);
    m_textureSize = {};
    m_geometryDirty = true;

    if (!m_texture && m_geometry.vertexCount() != 0)
        m_geometry.allocate(0);
    markDirty(DirtyGeometry | DirtyMaterial);
}

QSGTexture *RadiusTextureNode::texture() const
{
    return m_texture;
}

void RadiusTextureNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool RadiusTextureNode::ownsTexture() const
{
    return m_ownsTexture;
}

void RadiusTextureNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;
    m_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering RadiusTextureNode::filtering() const
{
    return m_material.filtering();
}

void RadiusTextureNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;
    m_material.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering RadiusTextureNode::mipmapFiltering() const
{
    return m_material.mipmapFiltering();
}

void RadiusTextureNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_material.anisotropyLevel() == level)
        return;
    m_material.setAnisotropyLevel(level);
    markDirty(DirtyMaterial);
}

QSGTexture::AnisotropyLevel RadiusTextureNode::anisotropyLevel() const
{
    return m_material.anisotropyLevel();
}

void RadiusTextureNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_texCoordMode == mode)
        return;
    m_texCoordMode = mode;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

RadiusTextureNode::TextureCoordinatesTransformMode
RadiusTextureNode::textureCoordinatesTransform() const
{
    return m_texCoordMode;
}

void RadiusTextureNode::setTextureProvider(QSGTextureProvider *provider)
{
    if (m_provider == provider)
        return;
    if (m_provider) {
        disconnect(m_provider, &QSGTextureProvider::textureChanged,
                   this, &RadiusTextureNode::handleTextureChanged);
    }

    if (m_ownsTexture) {
        setTextureInternal(nullptr);
        setOwnsTexture(false);
    }
    m_provider = provider;
    if (m_provider) {
        connect(m_provider, &QSGTextureProvider::textureChanged,
                this, &RadiusTextureNode::handleTextureChanged, Qt::DirectConnection);
    }
    setTextureInternal(m_provider ? m_provider->texture() : nullptr);
}

void RadiusTextureNode::setRect(const QRectF &rect)
{
    if (m_rect == rect)
        return;
    m_rect = rect;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setViewport(const QRectF &viewport)
{
    if (m_viewport == viewport)
        return;
    m_viewport = viewport;
    m_geometryDirty = true;
    markDirty(DirtyGeometry | DirtyMaterial);
}

void RadiusTextureNode::setSourceRect(const QRectF &rect)
{
    if (m_sourceRect == rect)
        return;
    m_sourceRect = rect;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setSourceOffset(const QPointF &offset)
{
    if (m_sourceOffset == offset)
        return;
    m_sourceOffset = offset;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::setRadius(qreal radius)
{
    if (m_radius == radius)
        return;
    m_radius = radius;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setTopLeftRadius(qreal radius)
{
    if (m_topLeftRadius == radius)
        return;
    m_topLeftRadius = radius;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setTopRightRadius(qreal radius)
{
    if (m_topRightRadius == radius)
        return;
    m_topRightRadius = radius;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setBottomLeftRadius(qreal radius)
{
    if (m_bottomLeftRadius == radius)
        return;
    m_bottomLeftRadius = radius;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setBottomRightRadius(qreal radius)
{
    if (m_bottomRightRadius == radius)
        return;
    m_bottomRightRadius = radius;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setAntialiasing(bool antialiasing)
{
    if (m_antialiasing == antialiasing)
        return;
    m_antialiasing = antialiasing;
    markDirty(DirtyMaterial);
}

void RadiusTextureNode::setSourceSizeScale(qreal widthScale, qreal heightScale)
{
    if (qFuzzyCompare(m_sourceWidthScale, widthScale)
            && qFuzzyCompare(m_sourceHeightScale, heightScale)) {
        return;
    }
    m_sourceWidthScale = widthScale;
    m_sourceHeightScale = heightScale;
    m_geometryDirty = true;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::preprocess()
{
    if (m_provider && m_texture != m_provider->texture())
        setTextureInternal(m_provider->texture());
    if (!m_texture || !m_rect.isValid()) {
        if (m_geometry.vertexCount() != 0) {
            m_geometry.allocate(0);
            markDirty(DirtyGeometry);
        }
        return;
    }

    if (auto *dynamicTexture = qobject_cast<QSGDynamicTexture *>(m_texture)) {
        if (dynamicTexture->updateTexture())
            markDirty(DirtyMaterial);
    }

    if (m_textureSize != m_texture->textureSize()) {
        m_textureSize = m_texture->textureSize();
        m_geometryDirty = true;
    }

    if (m_geometry.vertexCount() != 4) {
        m_geometry.allocate(4);
        m_geometryDirty = true;
    }
    if (m_geometryDirty)
        updateGeometry();
    updateMaterial();
}

void RadiusTextureNode::handleTextureChanged()
{
    QSGTexture *providerTexture = m_provider ? m_provider->texture() : nullptr;
    if (m_texture != providerTexture) {
        setTextureInternal(providerTexture);
        return;
    }

    m_textureSize = {};
    m_geometryDirty = true;
    markDirty(DirtyGeometry | DirtyMaterial);
}

QRectF RadiusTextureNode::effectiveSourceRect() const
{
    if (m_sourceRect.isValid())
        return m_sourceRect;
    if (!m_texture)
        return {};
    const QSize size = m_texture->textureSize();
    return QRectF(m_sourceOffset,
                  QSizeF(size.width() * m_sourceWidthScale,
                         size.height() * m_sourceHeightScale));
}

void RadiusTextureNode::updateGeometry()
{
    if (!m_texture || !m_rect.isValid())
        return;

    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    QRectF sourceRect = effectiveSourceRect();
    if (m_texCoordMode.testFlag(QSGImageNode::MirrorHorizontally)) {
        const qreal left = sourceRect.left();
        sourceRect.setLeft(sourceRect.right());
        sourceRect.setRight(left);
    }
    if (m_texCoordMode.testFlag(QSGImageNode::MirrorVertically)) {
        const qreal top = sourceRect.top();
        sourceRect.setTop(sourceRect.bottom());
        sourceRect.setBottom(top);
    }
    const QRectF uv = m_texture->convertToNormalizedSourceRect(sourceRect);
    auto *vertices = static_cast<RadiusTextureVertex *>(m_geometry.vertexData());
    vertices[0] = { float(m_rect.left()), float(m_rect.top()),
                    float(uv.left()), float(uv.top()),
                    float(m_rect.left() - viewport.left()),
                    float(m_rect.top() - viewport.top()) };
    vertices[1] = { float(m_rect.left()), float(m_rect.bottom()),
                    float(uv.left()), float(uv.bottom()),
                    float(m_rect.left() - viewport.left()),
                    float(m_rect.bottom() - viewport.top()) };
    vertices[2] = { float(m_rect.right()), float(m_rect.top()),
                    float(uv.right()), float(uv.top()),
                    float(m_rect.right() - viewport.left()),
                    float(m_rect.top() - viewport.top()) };
    vertices[3] = { float(m_rect.right()), float(m_rect.bottom()),
                    float(uv.right()), float(uv.bottom()),
                    float(m_rect.right() - viewport.left()),
                    float(m_rect.bottom() - viewport.top()) };

    m_geometry.markVertexDataDirty();
    m_geometryDirty = false;
    markDirty(DirtyGeometry);
}

void RadiusTextureNode::updateMaterial()
{
    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    const auto effective = [this](float radius) {
        return radius >= 0.0f ? radius : m_radius;
    };
    CornerRadii radii { effective(m_topLeftRadius), effective(m_topRightRadius),
                        effective(m_bottomRightRadius), effective(m_bottomLeftRadius) };
    radii = normalizedRadii(viewport.size(), radii);
    m_material.setRadii(QVector4D(float(radii.topLeft), float(radii.topRight),
                                  float(radii.bottomRight), float(radii.bottomLeft)));
    m_material.setViewportSize(QVector2D(float(viewport.width()), float(viewport.height())));
    m_material.setAntialiasing(m_antialiasing);
}

WAYLIB_SERVER_END_NAMESPACE
