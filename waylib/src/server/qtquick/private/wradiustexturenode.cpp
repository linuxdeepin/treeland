// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wradiustexturenode_p.h"

#include <QSGDynamicTexture>
#include <QSGTexture>
#include <QSGRendererInterface>
#include <QQuickWindow>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <private/qsgplaintexture_p.h>
#include <private/qsgadaptationlayer_p.h>
#include <private/qsgsoftwarepixmaptexture_p.h>
#include <rhi/qrhi.h>
#include <cstring>
#include <cmath>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {
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

struct RadiusTextureNode::RhiData {
    QRhi *rhi = nullptr;
    std::unique_ptr<QRhiBuffer> vertices;
    std::unique_ptr<QRhiBuffer> uniforms;
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> bindings;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    QVector<quint32> renderPassFormat;
    int sampleCount = 0;
    QRhiTexture *texture = nullptr;
    QSGTexture::Filtering filtering = QSGTexture::None;
    QSGTexture::Filtering mipmapFiltering = QSGTexture::None;
    bool ready = false;
};

RadiusTextureNode::RadiusTextureNode(QQuickWindow *window)
    : m_window(window)
{
    setFlag(UsePreprocess, true);
}

RadiusTextureNode::~RadiusTextureNode()
{
    releaseResources();
    if (m_ownsTexture)
        delete m_texture;
}

void RadiusTextureNode::setTexture(QSGTexture *texture)
{
    if (m_provider) {
        disconnect(m_provider, &QSGTextureProvider::textureChanged,
                   this, &RadiusTextureNode::handleTextureChanged);
        disconnect(m_provider, &QObject::destroyed,
                   this, &RadiusTextureNode::handleTextureChanged);
        m_provider = nullptr;
    }
    setTextureInternal(texture);
}

void RadiusTextureNode::setTextureInternal(QSGTexture *texture)
{
    if (m_texture == texture) {
        // A provider may reuse the wrapper while replacing its native texture.
        m_geometryDirty = true;
        markDirty(DirtyGeometry | DirtyMaterial);
        return;
    }

    if (m_ownsTexture)
        delete m_texture;
    m_texture = texture;
    m_textureSize = {};
    m_geometryDirty = true;

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
    if (m_filtering == filtering)
        return;
    m_filtering = filtering;
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering RadiusTextureNode::filtering() const
{
    return m_filtering;
}

void RadiusTextureNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_mipmapFiltering == filtering)
        return;
    m_mipmapFiltering = filtering;
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering RadiusTextureNode::mipmapFiltering() const
{
    return m_mipmapFiltering;
}

void RadiusTextureNode::setAnisotropyLevel(QSGTexture::AnisotropyLevel level)
{
    if (m_anisotropyLevel == level)
        return;
    m_anisotropyLevel = level;
    markDirty(DirtyMaterial);
}

QSGTexture::AnisotropyLevel RadiusTextureNode::anisotropyLevel() const
{
    return m_anisotropyLevel;
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
        disconnect(m_provider, &QObject::destroyed,
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
        connect(m_provider, &QObject::destroyed,
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
    if (!m_texture || !m_rect.isValid())
        return;

    if (auto *dynamicTexture = qobject_cast<QSGDynamicTexture *>(m_texture)) {
        if (dynamicTexture->updateTexture())
            markDirty(DirtyMaterial);
    }

    if (m_textureSize != m_texture->textureSize()) {
        m_textureSize = m_texture->textureSize();
        m_geometryDirty = true;
    }

    if (m_geometryDirty)
        updateGeometry();
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
    auto *vertices = m_vertices;
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

    m_geometryDirty = false;
    markDirty(DirtyGeometry);
}

QVector4D RadiusTextureNode::effectiveRadii() const
{
    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    const auto effective = [this](float radius) {
        return radius >= 0.0f ? radius : m_radius;
    };
    CornerRadii radii { effective(m_topLeftRadius), effective(m_topRightRadius),
                        effective(m_bottomRightRadius), effective(m_bottomLeftRadius) };
    radii = normalizedRadii(viewport.size(), radii);
    return QVector4D(float(radii.topLeft), float(radii.topRight),
                     float(radii.bottomRight), float(radii.bottomLeft));
}

QRectF RadiusTextureNode::rect() const
{
    return m_rect;
}

QSGRenderNode::RenderingFlags RadiusTextureNode::flags() const
{
    return BoundedRectRendering | DepthAwareRendering | NoExternalRendering;
}

QSGRenderNode::StateFlags RadiusTextureNode::changedStates() const
{
    return ViewportState | ScissorState;
}

void RadiusTextureNode::releaseResources()
{
    m_rhi.reset();
}

void RadiusTextureNode::prepare()
{
    if (!m_window || !m_window->rhi())
        return;
    if (m_rhi)
        m_rhi->ready = false;
    if (!m_texture || !m_rect.isValid() || !renderTarget() || !commandBuffer())
        return;

    auto *rhi = m_window->rhi();
    if (m_rhi && m_rhi->rhi != rhi)
        releaseResources();
    if (!m_rhi) {
        m_rhi = std::make_unique<RhiData>();
        m_rhi->rhi = rhi;
        m_rhi->vertices.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                            sizeof(m_vertices)));
        m_rhi->uniforms.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 112));
        if (!m_rhi->vertices->create() || !m_rhi->uniforms->create()) {
            releaseResources();
            return;
        }
    }
    auto &data = *m_rhi;
    auto *updates = rhi->nextResourceUpdateBatch();
    m_texture->setFiltering(m_filtering);
    m_texture->setMipmapFiltering(m_mipmapFiltering);
    m_texture->setAnisotropyLevel(m_anisotropyLevel);
    m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
    m_texture->commitTextureOperations(rhi, updates);
    auto *texture = m_texture->rhiTexture();
    if (!texture) {
        commandBuffer()->resourceUpdate(updates);
        return;
    }

    if (!data.sampler || data.filtering != m_filtering
            || data.mipmapFiltering != m_mipmapFiltering) {
        data.pipeline.reset();
        data.bindings.reset();
        const auto filter = m_filtering == QSGTexture::Linear ? QRhiSampler::Linear : QRhiSampler::Nearest;
        const auto mipmap = m_mipmapFiltering == QSGTexture::None ? QRhiSampler::None
            : (m_mipmapFiltering == QSGTexture::Linear ? QRhiSampler::Linear : QRhiSampler::Nearest);
        data.sampler.reset(rhi->newSampler(filter, filter, mipmap,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if (!data.sampler->create()) {
            data.sampler.reset();
            commandBuffer()->resourceUpdate(updates);
            return;
        }
        data.filtering = m_filtering;
        data.mipmapFiltering = m_mipmapFiltering;
    }
    if (!data.bindings || data.texture != texture) {
        data.pipeline.reset();
        data.bindings.reset(rhi->newShaderResourceBindings());
        data.bindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage
                | QRhiShaderResourceBinding::FragmentStage, data.uniforms.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                     texture, data.sampler.get())
        });
        if (!data.bindings->create()) {
            data.bindings.reset();
            commandBuffer()->resourceUpdate(updates);
            return;
        }
        data.texture = texture;
    }

    auto *target = renderTarget();
    const auto format = target->renderPassDescriptor()->serializedFormat();
    if (data.renderPassFormat != format || data.sampleCount != target->sampleCount())
        data.pipeline.reset();
    if (!data.pipeline) {
        auto loadShader = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
        };
        const auto vertexShader = loadShader(QStringLiteral(":/waylib/shaders/radiustexture.vert.qsb"));
        const auto fragmentShader = loadShader(QStringLiteral(":/waylib/shaders/radiustexture.frag.qsb"));
        if (!vertexShader.isValid() || !fragmentShader.isValid()) {
            commandBuffer()->resourceUpdate(updates);
            return;
        }
        data.pipeline.reset(rhi->newGraphicsPipeline());
        data.pipeline->setShaderStages({ { QRhiShaderStage::Vertex, vertexShader },
                                        { QRhiShaderStage::Fragment, fragmentShader } });
        QRhiVertexInputLayout layout;
        layout.setBindings({ { sizeof(Vertex) } });
        layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
                               { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
                               { 0, 2, QRhiVertexInputAttribute::Float2, 4 * sizeof(float) } });
        data.pipeline->setVertexInputLayout(layout);
        data.pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        data.pipeline->setFlags(QRhiGraphicsPipeline::UsesScissor | QRhiGraphicsPipeline::UsesStencilRef);
        data.pipeline->setSampleCount(target->sampleCount());
        data.pipeline->setDepthTest(true);
        data.pipeline->setDepthWrite(false);
        data.pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        data.pipeline->setTargetBlends({ blend });
        QRhiGraphicsPipeline::StencilOpState stencil;
        stencil.compareOp = QRhiGraphicsPipeline::Equal;
        data.pipeline->setStencilFront(stencil);
        data.pipeline->setStencilBack(stencil);
        data.pipeline->setStencilWriteMask(0);
        data.pipeline->setShaderResourceBindings(data.bindings.get());
        data.pipeline->setRenderPassDescriptor(target->renderPassDescriptor());
        if (!data.pipeline->create()) {
            data.pipeline.reset();
            commandBuffer()->resourceUpdate(updates);
            return;
        }
        data.renderPassFormat = format;
        data.sampleCount = target->sampleCount();
    }

    // std140 rounds the block containing the trailing float up to 16 bytes.
    struct alignas(16) Uniforms {
        float matrix[16];
        float radii[4];
        float params[4];
        float hasAlphaChannel;
    } uniforms {};
    static_assert(sizeof(Uniforms) == 112);
    const QMatrix4x4 combined = *projectionMatrix() * *matrix();
    std::memcpy(uniforms.matrix, combined.constData(), sizeof(uniforms.matrix));
    const QVector4D radii = effectiveRadii();
    uniforms.radii[0] = radii.x();
    uniforms.radii[1] = radii.y();
    uniforms.radii[2] = radii.z();
    uniforms.radii[3] = radii.w();
    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    uniforms.params[0] = viewport.width();
    uniforms.params[1] = viewport.height();
    uniforms.params[2] = inheritedOpacity();
    uniforms.params[3] = m_antialiasing ? 1.0f : 0.0f;
    uniforms.hasAlphaChannel = m_texture->hasAlphaChannel() ? 1.0f : 0.0f;
    // Refresh UVs even if a provider reused its wrapper with a different atlas region.
    updateGeometry();
    updates->updateDynamicBuffer(data.vertices.get(), 0, sizeof(m_vertices), m_vertices);
    updates->updateDynamicBuffer(data.uniforms.get(), 0, sizeof(uniforms), &uniforms);
    commandBuffer()->resourceUpdate(updates);
    data.ready = true;
}

void RadiusTextureNode::render(const RenderState *state)
{
    if (!m_window || !m_texture || !m_rect.isValid())
        return;
    if (m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        renderSoftware(state);
    else
        renderRhi(state);
}

void RadiusTextureNode::renderRhi(const RenderState *state)
{
    if (!m_rhi || !m_rhi->ready)
        return;
    auto &data = *m_rhi;
    if (data.pipeline->hasStencilTest() != state->stencilEnabled()) {
        data.pipeline->setStencilTest(state->stencilEnabled());
        if (!data.pipeline->create()) {
            data.pipeline.reset();
            data.ready = false;
            return;
        }
    }
    auto *cb = commandBuffer();
    const QSize size = renderTarget()->pixelSize();
    cb->setGraphicsPipeline(data.pipeline.get());
    cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
    if (state->scissorEnabled()) {
        const QRect scissor = state->scissorRect();
        cb->setScissor(QRhiScissor(scissor.x(), scissor.y(), scissor.width(), scissor.height()));
    } else {
        cb->setScissor(QRhiScissor(0, 0, size.width(), size.height()));
    }
    if (state->stencilEnabled())
        cb->setStencilRef(state->stencilValue());
    cb->setShaderResources(data.bindings.get());
    const QRhiCommandBuffer::VertexInput binding(data.vertices.get(), 0);
    cb->setVertexInput(0, 1, &binding);
    cb->draw(4);
}

void RadiusTextureNode::renderSoftware(const RenderState *state)
{
    auto *painter = static_cast<QPainter *>(m_window->rendererInterface()->getResource(
        m_window, QSGRendererInterface::PainterResource));
    if (!painter)
        return;
    QImage source;
    if (auto *plain = qobject_cast<QSGPlainTexture *>(m_texture))
        source = plain->image();
    else if (auto *layer = qobject_cast<QSGLayer *>(m_texture))
        source = layer->toImage();
    else if (QByteArrayView(m_texture->metaObject()->className()) == "QSGSoftwarePixmapTexture")
        source = static_cast<QSGSoftwarePixmapTexture *>(m_texture)->pixmap().toImage();
    if (source.isNull())
        return;
    if (!m_texture->hasAlphaChannel() && source.hasAlphaChannel())
        source = source.convertToFormat(QImage::Format_RGB32);

    const QRectF viewport = m_viewport.isValid() ? m_viewport : m_rect;
    const QVector4D radii = effectiveRadii();
    QPainterPath path;
    path.moveTo(viewport.left() + radii.x(), viewport.top());
    path.lineTo(viewport.right() - radii.y(), viewport.top());
    path.arcTo(QRectF(viewport.right() - 2 * radii.y(), viewport.top(),
                     2 * radii.y(), 2 * radii.y()), 90, -90);
    path.lineTo(viewport.right(), viewport.bottom() - radii.z());
    path.arcTo(QRectF(viewport.right() - 2 * radii.z(), viewport.bottom() - 2 * radii.z(),
                     2 * radii.z(), 2 * radii.z()), 0, -90);
    path.lineTo(viewport.left() + radii.w(), viewport.bottom());
    path.arcTo(QRectF(viewport.left(), viewport.bottom() - 2 * radii.w(),
                     2 * radii.w(), 2 * radii.w()), 270, -90);
    path.lineTo(viewport.left(), viewport.top() + radii.x());
    path.arcTo(QRectF(viewport.left(), viewport.top(), 2 * radii.x(), 2 * radii.x()), 180, -90);
    path.closeSubpath();

    // Paint the mask first, then SourceIn the image. This antialiases the
    // rounded edge, unlike QPainter's aliased path clipping.
    const QTransform transform = matrix()->toTransform();
    const qreal dpr = painter->device()->devicePixelRatioF();
    const qreal scaleX = std::hypot(transform.m11(), transform.m12()) * dpr;
    const qreal scaleY = std::hypot(transform.m21(), transform.m22()) * dpr;
    if (scaleX <= 0 || scaleY <= 0)
        return;
    const QSize pixelSize(qCeil(m_rect.width() * scaleX), qCeil(m_rect.height() * scaleY));
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return;
    image.fill(Qt::transparent);
    QPainter maskPainter(&image);
    maskPainter.scale(pixelSize.width() / m_rect.width(), pixelSize.height() / m_rect.height());
    maskPainter.translate(-m_rect.topLeft());
    maskPainter.setRenderHint(QPainter::Antialiasing, m_antialiasing);
    maskPainter.fillPath(path, Qt::white);
    maskPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    maskPainter.setRenderHint(QPainter::SmoothPixmapTransform, m_filtering == QSGTexture::Linear);
    if (m_texCoordMode.testFlag(QSGImageNode::MirrorHorizontally)) {
        maskPainter.translate(m_rect.left() + m_rect.right(), 0);
        maskPainter.scale(-1, 1);
    }
    if (m_texCoordMode.testFlag(QSGImageNode::MirrorVertically)) {
        maskPainter.translate(0, m_rect.top() + m_rect.bottom());
        maskPainter.scale(1, -1);
    }
    const QRectF uv = m_texture->convertToNormalizedSourceRect(effectiveSourceRect());
    const QRectF sourceRect(uv.x() * source.width(), uv.y() * source.height(),
                            uv.width() * source.width(), uv.height() * source.height());
    maskPainter.drawImage(m_rect, source, sourceRect);
    maskPainter.end();

    painter->save();
    painter->resetTransform();
    if (const QRegion *clip = state->clipRegion())
        painter->setClipRegion(*clip, Qt::IntersectClip);
    painter->setWorldTransform(transform);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setOpacity(inheritedOpacity());
    painter->setRenderHint(QPainter::SmoothPixmapTransform, m_filtering == QSGTexture::Linear);
    painter->drawImage(m_rect, image);
    painter->restore();
}

WAYLIB_SERVER_END_NAMESPACE
