// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "tsgradiusimagenode.h"

#include <private/qsgtexturematerial_p.h>

namespace {
struct ImageVertex
{
    float x, y;
    float tx, ty;

    // for vertex geometry
    void set(float nx, float ny, float ntx, float nty)
    {
        x = ny;
        y = nx;
        tx = nty;
        ty = ntx;
    }
};

struct RadiusImageVertex : public ImageVertex
{
    float alpha;

    void set(float nx, float ny, float ntx, float nty, float nalpha)
    {
        ImageVertex::set(nx, ny, ntx, nty);
        alpha = nalpha;
    }
};

const QSGGeometry::AttributeSet &radiusImageAttributeSet()
{
    static QSGGeometry::Attribute data[] = {
        QSGGeometry::Attribute::createWithAttributeType(0,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2,
                                                        1,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::UnknownAttribute),
    };
    static QSGGeometry::AttributeSet attrs = { 3, sizeof(RadiusImageVertex), data };
    return attrs;
}

struct SmoothImageVertex
{
    float x, y, u, v;
    float dx, dy, du, dv;
};

[[maybe_unused]] const QSGGeometry::AttributeSet &smoothImageAttributeSet()
{
    static QSGGeometry::Attribute data[] = {
        QSGGeometry::Attribute::createWithAttributeType(0,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::TexCoord1Attribute),
        QSGGeometry::Attribute::createWithAttributeType(3,
                                                        2,
                                                        QSGGeometry::FloatType,
                                                        QSGGeometry::TexCoord2Attribute)
    };
    static QSGGeometry::AttributeSet attrs = { 4, sizeof(SmoothImageVertex), data };
    return attrs;
}

} // namespace

class TSmoothTextureMaterialRhiShader : public QSGOpaqueTextureMaterialRhiShader
{
public:
    TSmoothTextureMaterialRhiShader();

    bool updateUniformData(RenderState &state,
                           QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override;
};

TSmoothTextureMaterialRhiShader::TSmoothTextureMaterialRhiShader()
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    : QSGOpaqueTextureMaterialRhiShader(1) // TODO: support multiview
#endif
{
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/radiussmoothtexture.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/radiussmoothtexture.frag.qsb"));
}

bool TSmoothTextureMaterialRhiShader::updateUniformData(RenderState &state,
                                                        QSGMaterial *newMaterial,
                                                        QSGMaterial *oldMaterial)
{
    bool changed = false;
    QByteArray *buf = state.uniformData();

    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(buf->data() + 64, &opacity, 4);
        changed = true;
    }

    changed |=
        QSGOpaqueTextureMaterialRhiShader::updateUniformData(state, newMaterial, oldMaterial);

    return changed;
}

TSGRadiusSmoothTextureMaterial::TSGRadiusSmoothTextureMaterial()
{
    setFlag(QSGTextureMaterial::Blending);
}

int TSGRadiusSmoothTextureMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    const qintptr diff = qintptr(this) - qintptr(other);
    return diff < 0 ? -1 : (diff > 0 ? 1 : 0);
}

QSGMaterialType *TSGRadiusSmoothTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *TSGRadiusSmoothTextureMaterial::createShader(
    [[maybe_unused]] QSGRendererInterface::RenderMode renderMode) const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Q_ASSERT_X(viewCount() == 1, __func__, "Multiview not supported now.");
#endif
    return new TSmoothTextureMaterialRhiShader();
}

TSGRadiusImageNode::TSGRadiusImageNode()
    : m_antialiasing(false)
    , m_dirtyGeometry(false)
{
    setFlag(QSGNode::UsePreprocess);

    m_node.setMaterial(&m_material);
    m_node.setOpaqueMaterial(&m_opaquematerial);

    m_node.setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
    m_node.setFlag(OwnsGeometry);

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("tradiusimage"));
#endif
}

void TSGRadiusImageNode::setRect(const QRectF &rect)
{
    if (rect == m_targetRect)
        return;

    m_targetRect = rect;
    m_dirtyGeometry = true;
}

void TSGRadiusImageNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);

    if (m_material.texture() != texture) {
        m_material.setTexture(texture);
        m_opaquematerial.setTexture(texture);
        m_radiusMaterial.setTexture(texture);

        setMipmapFiltering(texture->mipmapFiltering());
        setFiltering(texture->filtering());
        setHorizontalWrapMode(texture->horizontalWrapMode());
        setVerticalWrapMode(texture->verticalWrapMode());
    }

    updateMaterialBlending();
}

QSGTexture *TSGRadiusImageNode::texture() const
{
    return m_material.texture();
}

void TSGRadiusImageNode::setAntialiasingWidth(float width)
{
    if (m_antialiasingWidth == width)
        return;

    m_antialiasingWidth = width;
    m_dirtyGeometry = true;
}

void TSGRadiusImageNode::preprocess()
{
    bool doDirty = false;
    if (m_provider && m_provider->texture()) {
        setTexture(m_provider->texture());
        if (QSGDynamicTexture *dt = qobject_cast<QSGDynamicTexture *>(texture())) {
            doDirty = dt->updateTexture();
        }

        if (m_textureSize != texture()->textureSize()) {
            m_dirtyGeometry = true;
            m_textureSize = texture()->textureSize();
        }

        if (doDirty)
            markDirty(DirtyMaterial);

        if (m_dirtyGeometry) {
            updateGeometry();
            m_dirtyGeometry = false;
        }
    }

    if (m_node.parent() && !m_provider->texture()) {
        removeChildNode(&m_node);
    } else if (!m_node.parent() && m_provider->texture()) {
        appendChildNode(&m_node);
    }
}

void TSGRadiusImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_opaquematerial.setFiltering(filtering);
    m_radiusMaterial.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

void TSGRadiusImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    m_opaquematerial.setMipmapFiltering(filtering);
    m_radiusMaterial.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

void TSGRadiusImageNode::setVerticalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.verticalWrapMode() == wrapMode)
        return;

    m_material.setVerticalWrapMode(wrapMode);
    m_opaquematerial.setVerticalWrapMode(wrapMode);
    m_radiusMaterial.setVerticalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void TSGRadiusImageNode::setHorizontalWrapMode(QSGTexture::WrapMode wrapMode)
{
    if (m_material.horizontalWrapMode() == wrapMode)
        return;

    m_material.setHorizontalWrapMode(wrapMode);
    m_opaquematerial.setHorizontalWrapMode(wrapMode);
    m_radiusMaterial.setHorizontalWrapMode(wrapMode);
    markDirty(DirtyMaterial);
}

void TSGRadiusImageNode::setRadius(qreal radius)
{
    if (radius != m_radius) {
        m_radius = radius;
        m_dirtyGeometry = true;
    }
}

void TSGRadiusImageNode::setTopLeftRadius(qreal radius)
{
    if (radius != m_topLeftRadius) {
        m_topLeftRadius = radius;
        m_dirtyGeometry = true;
    }
}

void TSGRadiusImageNode::setTopRightRadius(qreal radius)
{
    if (radius != m_topRightRadius) {
        m_topRightRadius = radius;
        m_dirtyGeometry = true;
    }
}

void TSGRadiusImageNode::setBottomLeftRadius(qreal radius)
{
    if (radius != m_bottomLeftRadius) {
        m_bottomLeftRadius = radius;
        m_dirtyGeometry = true;
    }
}

void TSGRadiusImageNode::setBottomRightRadius(qreal radius)
{
    if (radius != m_bottomRightRadius) {
        m_bottomRightRadius = radius;
        m_dirtyGeometry = true;
    }
}

void TSGRadiusImageNode::setAntialiasing(bool antialiasing)
{
    if (antialiasing == m_antialiasing)
        return;

    m_antialiasing = antialiasing;
    if (m_radius > 0 || m_topLeftRadius > 0 || m_topRightRadius > 0 || m_bottomLeftRadius > 0
        || m_bottomRightRadius > 0) {
        m_node.setGeometry(new QSGGeometry(radiusImageAttributeSet(), 0));
    } else {
        m_node.setGeometry(new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4));
    }
    m_node.setFlag(OwnsGeometry);

    updateMaterialAntialiasing();
    m_dirtyGeometry = true;
}

void TSGRadiusImageNode::setTextureProvider(QSGTextureProvider *p)
{
    if (p != m_provider) {
        if (m_provider) {
            disconnect(m_provider.data(),
                       &QSGTextureProvider::textureChanged,
                       this,
                       &TSGRadiusImageNode::handleTextureChange);
        }

        m_provider = p;
        connect(m_provider.data(),
                &QSGTextureProvider::textureChanged,
                this,
                &TSGRadiusImageNode::handleTextureChange,
                Qt::DirectConnection);
    }
}

void TSGRadiusImageNode::handleTextureChange()
{
    markDirty(QSGNode::DirtyMaterial);
}

void TSGRadiusImageNode::updateMaterialAntialiasing()
{
    if (m_radius > 0 || m_topLeftRadius > 0 || m_topRightRadius > 0 || m_bottomLeftRadius > 0
        || m_bottomRightRadius > 0) {
        m_node.setMaterial(&m_radiusMaterial);
        m_node.setOpaqueMaterial(nullptr);
    } else {
        m_node.setMaterial(&m_material);
        m_node.setOpaqueMaterial(&m_opaquematerial);
    }
}

void TSGRadiusImageNode::setMaterialTexture(QSGTexture *texture)
{
    m_material.setTexture(texture);
    m_opaquematerial.setTexture(texture);
    m_radiusMaterial.setTexture(texture);
}

bool TSGRadiusImageNode::updateMaterialBlending()
{
    const bool alpha = m_material.flags() & QSGMaterial::Blending;

    if (m_radiusMaterial.texture()) {
        m_radiusMaterial.setFlag(QSGMaterial::Blending, true);
    }

    if (texture() && alpha != texture()->hasAlphaChannel()) {
        m_opaquematerial.setFlag(QSGMaterial::Blending, !alpha);
        return true;
    }

    return true;
}

void TSGRadiusImageNode::updateGeometry()
{
    QRectF textRect = QRectF(0, 0, 1, 1);
    if (m_radius > 0 || m_topLeftRadius > 0 || m_topRightRadius > 0 || m_bottomLeftRadius > 0
        || m_bottomRightRadius > 0) {
        updateTexturedRadiusGeometry(m_targetRect, textRect);
    } else {
        QSGGeometry::updateTexturedRectGeometry(m_node.geometry(), m_targetRect, textRect);
    }
    m_node.markDirty(QSGNode::DirtyGeometry);
}

void TSGRadiusImageNode::updateTexturedRadiusGeometry(const QRectF &rect, [[maybe_unused]] const QRectF &textureRect)
{
    float width = float(rect.width());
    float height = float(rect.height());

    QSGGeometry *g = m_node.geometry();
    g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
    int vertexStride = g->sizeOfVertex();

    union
    {
        ImageVertex *vertices;
        RadiusImageVertex *smoothVertices;
    };

    float radiusTL = qMin(qMin(width, height) * 0.4999f,
                          float(m_topLeftRadius < 0 ? m_radius : m_topLeftRadius));
    float radiusTR = qMin(qMin(width, height) * 0.4999f,
                          float(m_topRightRadius < 0 ? m_radius : m_topRightRadius));
    float radiusBL = qMin(qMin(width, height) * 0.4999f,
                          float(m_bottomLeftRadius < 0 ? m_radius : m_bottomLeftRadius));
    float radiusBR = qMin(qMin(width, height) * 0.4999f,
                          float(m_bottomRightRadius < 0 ? m_radius : m_bottomRightRadius));

    if (radiusTL <= 0.5)
        radiusTL = 0;
    if (radiusTR <= 0.5)
        radiusTR = 0;
    if (radiusBL <= 0.5)
        radiusBL = 0;
    if (radiusBR <= 0.5)
        radiusBR = 0;

    const float innerRadiusTL = qMax(radiusTL, 0.01);
    const float innerRadiusTR = qMax(radiusTR, 0.01);
    const float innerRadiusBL = qMax(radiusBL, 0.01);
    const float innerRadiusBR = qMax(radiusBR, 0.01);
    const float outerRadiusTL = radiusTL;
    const float outerRadiusTR = radiusTR;
    const float outerRadiusBL = radiusBL;
    const float outerRadiusBR = radiusBR;

    int segmentsTL = radiusTL == 0 ? 0 : qBound(3, qCeil(radiusTL * (M_PI / 6)), 18);
    int segmentsTR = radiusTR == 0 ? 0 : qBound(3, qCeil(radiusTR * (M_PI / 6)), 18);
    int segmentsBL = radiusBL == 0 ? 0 : qBound(3, qCeil(radiusBL * (M_PI / 6)), 18);
    int segmentsBR = radiusBR == 0 ? 0 : qBound(3, qCeil(radiusBR * (M_PI / 6)), 18);

    if (innerRadiusTL == innerRadiusTR) {
        if (segmentsTL <= segmentsTR)
            segmentsTL = 0;
        else
            segmentsTR = 0;
    }
    if (innerRadiusBL == innerRadiusBR) {
        if (segmentsBL <= segmentsBR)
            segmentsBL = 0;
        else
            segmentsBR = 0;
    }

    const int sumSegments = segmentsTL + segmentsTR + segmentsBL + segmentsBR;
    const int innerVertexCount = (sumSegments + 4) * 2;
    int vertexCount = innerVertexCount;
    if (m_antialiasing)
        vertexCount += innerVertexCount;

    const int fillIndexCount = innerVertexCount;
    const int innerAAIndexCount = innerVertexCount * 2 + 2;
    int indexCount = 0;
    int fillHead = 0;
    int innerAAHead = 0;
    int innerAATail = 0;
    bool hasFill = true;
    if (hasFill)
        indexCount += fillIndexCount;
    if (m_antialiasing) {
        innerAATail = innerAAHead = indexCount + (innerAAIndexCount >> 1) + 1;
        indexCount += innerAAIndexCount;
    }

    g->allocate(vertexCount, indexCount);
    vertices = reinterpret_cast<ImageVertex *>(g->vertexData());
    memset(vertices, 0, vertexCount * vertexStride);
    quint16 *indices = g->indexDataAsUShort();
    quint16 index = 0;

    float innerXPrev = 0.;
    float innerYLeftPrev = 0.;
    float innerYRightPrev = 0.;

    const float angleTL = 0.5f * float(M_PI) / segmentsTL;
    const float cosStepTL = qFastCos(angleTL);
    const float sinStepTL = qFastSin(angleTL);
    const float angleTR = 0.5f * float(M_PI) / segmentsTR;
    const float cosStepTR = qFastCos(angleTR);
    const float sinStepTR = qFastSin(angleTR);
    const float angleBL = 0.5f * float(M_PI) / segmentsBL;
    const float cosStepBL = qFastCos(angleBL);
    const float sinStepBL = qFastSin(angleBL);
    const float angleBR = 0.5f * float(M_PI) / segmentsBR;
    const float cosStepBR = qFastCos(angleBR);
    const float sinStepBR = qFastSin(angleBR);

    const float outerXCenter[][2] = {
        { float(rect.top() + radiusTL), float(rect.top() + radiusTR) },
        { float(rect.bottom() - radiusBL), float(rect.bottom() - radiusBR) }
    };
    const float outerYCenter[][2] = {
        { float(rect.left() + outerRadiusTL), float(rect.right() - outerRadiusTR) },
        { float(rect.left() + outerRadiusBL), float(rect.right() - outerRadiusBR) }
    };
    const float innerXCenter[][2] = {
        { float(rect.top() + innerRadiusTL), float(rect.top() + innerRadiusTR) },
        { float(rect.bottom() - innerRadiusBL), float(rect.bottom() - innerRadiusBR) }
    };
    const float innerYCenter[][2] = {
        { float(rect.left() + innerRadiusTL), float(rect.right() - innerRadiusTR) },
        { float(rect.left() + innerRadiusBL), float(rect.right() - innerRadiusBR) }
    };
    const float innerRadius[][2] = { { innerRadiusTL, innerRadiusTR },
                                     { innerRadiusBL, innerRadiusBR } };
    const float outerRadius[][2] = { { outerRadiusTL, outerRadiusTR },
                                     { outerRadiusBL, outerRadiusBR } };
    const int segments[][2] = { { segmentsTL, segmentsTR }, { segmentsBL, segmentsBR } };
    const float cosStep[][2] = { { cosStepTL, cosStepTR }, { cosStepBL, cosStepBR } };
    const float sinStep[][2] = { { sinStepTL, sinStepTR }, { sinStepBL, sinStepBR } };

    float cosSegmentAngleLeft;
    float sinSegmentAngleLeft;
    float cosSegmentAngleRight;
    float sinSegmentAngleRight;
    bool advanceLeft = true;

    float xLeft, yLeft, xRight, yRight;
    float outerXLeft, outerYLeft, outerXRight, outerYRight;
    float sinAngleLeft, cosAngleLeft, sinAngleRight, cosAngleRight;
    float antiOuterXRight, antiOuterYRight, antiOuterXLeft, antiOuterYLeft;
    qreal tmpLeft, tmpRight;

    for (int part = 0; part < 2; ++part) {
        cosSegmentAngleLeft = 1. - part;
        sinSegmentAngleLeft = part;
        cosSegmentAngleRight = 1. - part;
        sinSegmentAngleRight = part;
        advanceLeft = true;

        for (int iLeft = 0, iRight = 0;
             iLeft <= segments[part][0] || iRight <= segments[part][1];) {
            xLeft = innerXCenter[part][0] - innerRadius[part][0] * cosSegmentAngleLeft;
            xRight = innerXCenter[part][1] - innerRadius[part][1] * cosSegmentAngleRight;

            yLeft = innerYCenter[part][0] - innerRadius[part][0] * sinSegmentAngleLeft;
            yRight = innerYCenter[part][1] + innerRadius[part][1] * sinSegmentAngleRight;

            if ((iLeft <= segments[part][0] && xLeft <= xRight) || iRight > segments[part][1]) {
                advanceLeft = true;
            } else {
                advanceLeft = false;
            }

            if (innerRadius[part][0] == innerRadius[part][1]) {
                if (advanceLeft) {
                    if (outerRadius[part][0] == 0) {
                        sinAngleLeft = 1.;
                        cosAngleLeft = part ? -1. : 1.;
                    } else {
                        sinAngleLeft = sinSegmentAngleLeft;
                        cosAngleLeft = cosSegmentAngleLeft;
                    }
                    if (outerRadius[part][1] == 0) {
                        sinAngleRight = 1.;
                        cosAngleRight = part ? -1. : 1.;
                    } else {
                        sinAngleRight = sinSegmentAngleLeft;
                        cosAngleRight = cosSegmentAngleLeft;
                    }
                    xRight = xLeft;
                    yRight = innerYCenter[part][1] + innerRadius[part][1] * sinAngleRight;
                } else {
                    if (outerRadius[part][0] == 0) {
                        sinAngleLeft = 1.;
                        cosAngleLeft = part ? -1. : 1.;
                    } else {
                        sinAngleLeft = sinSegmentAngleRight;
                        cosAngleLeft = cosSegmentAngleRight;
                    }
                    if (outerRadius[part][1] == 0) {
                        sinAngleRight = 1.;
                        cosAngleRight = part ? -1. : 1.;
                    } else {
                        sinAngleRight = sinSegmentAngleRight;
                        cosAngleRight = cosSegmentAngleRight;
                    }
                    xLeft = xRight;
                    yLeft = innerYCenter[part][0] - innerRadius[part][0] * sinAngleLeft;
                }
            } else if (advanceLeft) {
                if (outerRadius[part][0] == 0) {
                    sinAngleLeft = 1.;
                    cosAngleLeft = part ? -1. : 1.;
                } else {
                    sinAngleLeft = sinSegmentAngleLeft;
                    cosAngleLeft = cosSegmentAngleLeft;
                }
                if (outerRadius[part][1] == 0) {
                    sinAngleRight = 1.;
                    cosAngleRight = part ? -1. : 1.;
                    xRight = xLeft;
                    yRight = innerYCenter[part][1] + innerRadius[part][1] * sinAngleRight;
                } else if (xLeft >= innerXCenter[0][1] && xLeft <= innerXCenter[1][1]) {
                    sinAngleRight = 1.;
                    cosAngleRight = 0.;
                    xRight = xLeft;
                    yRight = innerYCenter[part][1] + innerRadius[part][1] * sinAngleRight;
                } else {
                    if (xRight != innerXPrev) {
                        float t = (xLeft - innerXPrev) / (xRight - innerXPrev);
                        yRight = innerYRightPrev * (1. - t) + yRight * t;
                        xRight = xLeft;
                    }
                    sinAngleRight = (yRight - innerYCenter[part][1]) / innerRadius[part][1];
                    cosAngleRight = -(xRight - innerXCenter[part][1]) / innerRadius[part][1];
                }
            } else {
                if (outerRadius[part][1] == 0) {
                    sinAngleRight = 1.;
                    cosAngleRight = part ? -1. : 1.;
                } else {
                    sinAngleRight = sinSegmentAngleRight;
                    cosAngleRight = cosSegmentAngleRight;
                }
                if (outerRadius[part][0] == 0) {
                    sinAngleLeft = 1.;
                    cosAngleLeft = part ? -1. : 1.;
                    xLeft = xRight;
                    yLeft = innerYCenter[part][0] - innerRadius[part][0] * sinAngleLeft;
                } else if (xRight >= innerXCenter[0][0] && xRight <= innerXCenter[1][0]) {
                    sinAngleLeft = 1.;
                    cosAngleLeft = 0.;
                    xLeft = xRight;
                    yLeft = innerYCenter[part][0] - innerRadius[part][0] * sinAngleLeft;
                } else {
                    if (xLeft != innerXPrev) {
                        float t = (xRight - innerXPrev) / (xLeft - innerXPrev);
                        yLeft = innerYLeftPrev * (1. - t) + yLeft * t;
                        xLeft = xRight;
                    }
                    sinAngleLeft = -(yLeft - innerYCenter[part][0]) / innerRadius[part][0];
                    cosAngleLeft = -(xLeft - innerXCenter[part][0]) / innerRadius[part][0];
                }
            }

            outerXLeft = outerXCenter[part][0] - outerRadius[part][0] * cosAngleLeft;
            outerYLeft = outerYCenter[part][0] - outerRadius[part][0] * sinAngleLeft;
            outerXRight = outerXCenter[part][1] - outerRadius[part][1] * cosAngleRight;
            outerYRight = outerYCenter[part][1] + outerRadius[part][1] * sinAngleRight;

            if (hasFill) {
                indices[fillHead++] = index;
                indices[fillHead++] = index + 1;
            }

            if (m_antialiasing) {
                indices[--innerAAHead] = index + 2;
                indices[--innerAAHead] = index;
                indices[innerAATail++] = index + 1;
                indices[innerAATail++] = index + 3;

                smoothVertices[index++].set(outerXRight,
                                            outerYRight,
                                            outerXRight / height,
                                            outerYRight / width,
                                            1.0f);
                smoothVertices[index++].set(outerXLeft,
                                            outerYLeft,
                                            outerXLeft / height,
                                            outerYLeft / width,
                                            1.0f);
                antiOuterXRight = xRight - m_antialiasingWidth * cosAngleRight;
                antiOuterYRight = yRight + m_antialiasingWidth * sinAngleRight;
                antiOuterXLeft = xLeft - m_antialiasingWidth * cosAngleLeft;
                antiOuterYLeft = yLeft - m_antialiasingWidth * sinAngleLeft;
                smoothVertices[index++].set(antiOuterXRight,
                                            antiOuterYRight,
                                            antiOuterXRight / height,
                                            antiOuterYRight / width,
                                            0.0f);
                smoothVertices[index++].set(antiOuterXLeft,
                                            antiOuterYLeft,
                                            antiOuterXLeft / height,
                                            antiOuterYLeft / width,
                                            0.0f);
            } else {
                vertices[index++].set(xRight, yRight, xRight / height, yRight / width);
                vertices[index++].set(xLeft, yLeft, xLeft / height, yLeft / width);
            }

            innerXPrev = xLeft;
            innerYLeftPrev = yLeft;
            innerYRightPrev = yRight;

            if (advanceLeft) {
                iLeft++;
                tmpLeft = cosSegmentAngleLeft;
                cosSegmentAngleLeft =
                    cosSegmentAngleLeft * cosStep[part][0] - sinSegmentAngleLeft * sinStep[part][0];
                sinSegmentAngleLeft =
                    sinSegmentAngleLeft * cosStep[part][0] + tmpLeft * sinStep[part][0];
            } else {
                iRight++;
                tmpRight = cosSegmentAngleRight;
                cosSegmentAngleRight = cosSegmentAngleRight * cosStep[part][1]
                    - sinSegmentAngleRight * sinStep[part][1];
                sinSegmentAngleRight =
                    sinSegmentAngleRight * cosStep[part][1] + tmpRight * sinStep[part][1];
            }
        }
    }

    Q_ASSERT(index == vertexCount);

    if (m_antialiasing) {
        indices[--innerAAHead] = indices[innerAATail - 1];
        indices[--innerAAHead] = indices[innerAATail - 2];
        Q_ASSERT(innerAATail <= indexCount);
    }
}
