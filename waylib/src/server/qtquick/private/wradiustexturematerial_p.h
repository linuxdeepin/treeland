// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QSGTextureMaterial>
#include <QSGTexture>
#include <QRectF>
#include <QVector2D>
#include <QVector4D>

WAYLIB_SERVER_BEGIN_NAMESPACE

class RadiusTextureMaterial final : public QSGTextureMaterial
{
public:
    RadiusTextureMaterial();

    QSGMaterialType *type() const override;
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode renderMode) const override;
    int compare(const QSGMaterial *other) const override;

    void setTexture(QSGTexture *texture);

    QVector4D radii() const;
    void setRadii(const QVector4D &radii);

    QVector2D viewportSize() const;
    void setViewportSize(const QVector2D &size);

    bool antialiasing() const;
    void setAntialiasing(bool antialiasing);

private:
    QVector4D m_radii;
    QVector2D m_viewportSize;
    bool m_antialiasing = true;
};

WAYLIB_SERVER_END_NAMESPACE
