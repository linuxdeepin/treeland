// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QPointer>
#include <QSGGeometryNode>
#include <QSGTextureMaterial>
#include <QSGTextureProvider>

class TSGRadiusSmoothTextureMaterial : public QSGOpaqueTextureMaterial
{
public:
    TSGRadiusSmoothTextureMaterial();
    int compare(const QSGMaterial *other) const override;

protected:
    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
};

class TSGRadiusImageNode
    : public QObject
    , public QSGNode
{
    Q_OBJECT
public:
    TSGRadiusImageNode();

    void setRect(const QRectF &rect);
    void setAntialiasingWidth(float width);

    void preprocess() override;
    void setMipmapFiltering(QSGTexture::Filtering filtering);
    void setFiltering(QSGTexture::Filtering filtering);
    void setHorizontalWrapMode(QSGTexture::WrapMode wrapMode);
    void setVerticalWrapMode(QSGTexture::WrapMode wrapMode);

    void setRadius(qreal radius);
    void setTopLeftRadius(qreal radius);
    void setTopRightRadius(qreal radius);
    void setBottomLeftRadius(qreal radius);
    void setBottomRightRadius(qreal radius);

    void setAntialiasing(bool antialiasing);
    void setTextureProvider(QSGTextureProvider *p);

public Q_SLOTS:
    void handleTextureChange();

protected:
    void updateMaterialAntialiasing();
    void setMaterialTexture(QSGTexture *texture);
    bool updateMaterialBlending();
    void updateGeometry();
    void updateTexturedRadiusGeometry(const QRectF &rect, const QRectF &textureRect);

private:
    void setTexture(QSGTexture *texture);
    QSGTexture *texture() const;

private:
    QSGGeometryNode m_node;

    QSGOpaqueTextureMaterial m_opaquematerial;
    QSGTextureMaterial m_material;
    TSGRadiusSmoothTextureMaterial m_radiusMaterial;

    QPointer<QSGTextureProvider> m_provider;

    QRectF m_targetRect;
    QSize m_textureSize;

    float m_radius = 0.0f;
    float m_topLeftRadius = -1.0f;
    float m_topRightRadius = -1.0f;
    float m_bottomLeftRadius = -1.0f;
    float m_bottomRightRadius = -1.0f;
    float m_antialiasingWidth = 1;

    uint m_antialiasing : 1;
    uint m_dirtyGeometry : 1;
};
