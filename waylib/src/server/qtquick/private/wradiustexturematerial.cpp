// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wradiustexturematerial_p.h"

#include <QMatrix4x4>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <rhi/qrhi.h>

#include <cstring>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
bool isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

class RadiusTextureMaterialShader final : public QSGMaterialShader
{
public:
    RadiusTextureMaterialShader()
    {
        setShaderFileName(VertexStage,
                          QStringLiteral(":/waylib/shaders/radiustexture.vert.qsb"));
        setShaderFileName(FragmentStage,
                          QStringLiteral(":/waylib/shaders/radiustexture.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial,
                           QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);

        const auto *material = static_cast<RadiusTextureMaterial *>(newMaterial);
        struct UniformData {
            float matrix[16];
            float radii[4];
            float params[4];
            float hasAlphaChannel;
        } uniformData;

        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(uniformData.matrix, matrix.constData(), sizeof(uniformData.matrix));

        const QVector4D radii = material->radii();
        uniformData.radii[0] = radii.x();
        uniformData.radii[1] = radii.y();
        uniformData.radii[2] = radii.z();
        uniformData.radii[3] = radii.w();

        const QVector2D viewportSize = material->viewportSize();
        uniformData.params[0] = viewportSize.x();
        uniformData.params[1] = viewportSize.y();
        uniformData.params[2] = state.opacity();
        uniformData.params[3] = material->antialiasing() ? 1.0f : 0.0f;
        const auto *texture = material->texture();
        uniformData.hasAlphaChannel = texture && texture->hasAlphaChannel() ? 1.0f : 0.0f;

        QByteArray *uniformBuffer = state.uniformData();
        Q_ASSERT(uniformBuffer->size() >= qsizetype(sizeof(uniformData)));
        std::memcpy(uniformBuffer->data(), &uniformData, sizeof(uniformData));
        return true;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override
    {
        Q_UNUSED(oldMaterial);
        if (binding != 1)
            return;

        const auto *material = static_cast<RadiusTextureMaterial *>(newMaterial);
        auto *sourceTexture = material->texture();
        if (!sourceTexture) {
            *texture = nullptr;
            return;
        }

        sourceTexture->setFiltering(material->filtering());
        sourceTexture->setMipmapFiltering(material->mipmapFiltering());
        sourceTexture->setAnisotropyLevel(material->anisotropyLevel());
        sourceTexture->setHorizontalWrapMode(material->horizontalWrapMode());
        sourceTexture->setVerticalWrapMode(material->verticalWrapMode());
        if (!state.rhi()->isFeatureSupported(QRhi::NPOTTextureRepeat)) {
            const QSize size = sourceTexture->textureSize();
            if (!isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height())) {
                sourceTexture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
                sourceTexture->setVerticalWrapMode(QSGTexture::ClampToEdge);
                sourceTexture->setMipmapFiltering(QSGTexture::None);
            }
        }
        sourceTexture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = sourceTexture;
    }
};
}

RadiusTextureMaterial::RadiusTextureMaterial()
{
    setFlag(Blending);
    setFlag(NoBatching);
}

QSGMaterialType *RadiusTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader *RadiusTextureMaterial::createShader(
    QSGRendererInterface::RenderMode renderMode) const
{
    Q_UNUSED(renderMode);
    return new RadiusTextureMaterialShader;
}

int RadiusTextureMaterial::compare(const QSGMaterial *other) const
{
    Q_ASSERT(other && type() == other->type());
    if (this == other)
        return 0;
    return reinterpret_cast<quintptr>(this) < reinterpret_cast<quintptr>(other) ? -1 : 1;
}

void RadiusTextureMaterial::setTexture(QSGTexture *texture)
{
    QSGTextureMaterial::setTexture(texture);
    setFlag(Blending);
}

QVector4D RadiusTextureMaterial::radii() const
{
    return m_radii;
}

void RadiusTextureMaterial::setRadii(const QVector4D &radii)
{
    m_radii = radii;
}

QVector2D RadiusTextureMaterial::viewportSize() const
{
    return m_viewportSize;
}

void RadiusTextureMaterial::setViewportSize(const QVector2D &size)
{
    m_viewportSize = size;
}

bool RadiusTextureMaterial::antialiasing() const
{
    return m_antialiasing;
}

void RadiusTextureMaterial::setAntialiasing(bool antialiasing)
{
    m_antialiasing = antialiasing;
}

WAYLIB_SERVER_END_NAMESPACE
