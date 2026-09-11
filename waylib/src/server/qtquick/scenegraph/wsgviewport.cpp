// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wsgviewport_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

static bool sameReal(qreal a, qreal b)
{
    return qFuzzyCompare(a + 1.0, b + 1.0);
}

static bool sameRectF(const QRectF &a, const QRectF &b)
{
    return a.isNull() == b.isNull()
        && sameReal(a.x(), b.x()) && sameReal(a.y(), b.y())
        && sameReal(a.width(), b.width()) && sameReal(a.height(), b.height());
}

static bool sameMatrix(const QMatrix4x4 &a, const QMatrix4x4 &b)
{
    const float *da = a.constData();
    const float *db = b.constData();
    for (int i = 0; i < 16; ++i) {
        if (!qFuzzyCompare(qreal(da[i]) + 1.0, qreal(db[i]) + 1.0))
            return false;
    }
    return true;
}

WSGViewport::WSGViewport() = default;

void WSGViewport::setDevicePixelRatio(qreal devicePixelRatio)
{
    if (sameReal(m_devicePixelRatio, devicePixelRatio))
        return;
    m_devicePixelRatio = devicePixelRatio;
    m_damageRegion = WDamageRegion(true);
}

void WSGViewport::setRenderParameters(const QMatrix4x4 &renderMatrix,
                                      const QRectF &sourceRect,
                                      const QRectF &targetRect)
{
    if (sameMatrix(m_renderMatrix, renderMatrix)
        && sameRectF(m_sourceRect, sourceRect)
        && sameRectF(m_targetRect, targetRect))
        return;
    m_renderMatrix = renderMatrix;
    m_sourceRect = sourceRect;
    m_targetRect = targetRect;
    m_damageRegion = WDamageRegion(true);
}

qreal WSGViewport::devicePixelRatio() const
{
    return m_devicePixelRatio;
}

const QMatrix4x4 &WSGViewport::renderMatrix() const
{
    return m_renderMatrix;
}

QRectF WSGViewport::sourceRect() const
{
    return m_sourceRect;
}

QRectF WSGViewport::targetRect() const
{
    return m_targetRect;
}

QMatrix4x4 WSGViewport::sceneToBufferTransform(const QSize &pixelSize) const
{
    QMatrix4x4 scale;
    scale.scale(m_devicePixelRatio, m_devicePixelRatio);
    const QMatrix4x4 inputMap(inputMapToOutput(m_sourceRect, m_targetRect,
                                               pixelSize, m_devicePixelRatio));
    return scale * (inputMap * m_renderMatrix);
}

QRect WSGViewport::sceneOutputRect(const QSize &pixelSize) const
{
    if (pixelSize.isEmpty() || !qIsFinite(m_devicePixelRatio) || m_devicePixelRatio <= 0)
        return {};

    bool invertible = false;
    const QMatrix4x4 fromBuffer = sceneToBufferTransform(pixelSize).inverted(&invertible);
    if (!invertible)
        return {};
    for (int i = 0; i < 16; ++i) {
        if (!qIsFinite(fromBuffer.constData()[i]))
            return {};
    }

    const QRectF sceneRect = fromBuffer.mapRect(QRectF(QPointF(), QSizeF(pixelSize)));
    if (!qIsFinite(sceneRect.left()) || !qIsFinite(sceneRect.top())
        || !qIsFinite(sceneRect.right()) || !qIsFinite(sceneRect.bottom()))
        return {};
    return outerAligned(sceneRect);
}

bool WSGViewport::affectsBuffer(const QSize &pixelSize) const
{
    if (isFull())
        return true;
    if (m_damageRegion.isEmpty())
        return false;
    const QRect clip = sceneOutputRect(pixelSize);
    if (clip.isEmpty())
        return true;
    return !(m_damageRegion.region & clip).isEmpty();
}

QTransform WSGViewport::inputMapToOutput(const QRectF &sourceRect, const QRectF &targetRect,
                                        const QSize &pixelSize, const qreal devicePixelRatio)
{
    Q_ASSERT(pixelSize.isValid());

    QTransform t;
    const auto outputSize = QSizeF(pixelSize) / devicePixelRatio;

    if (sourceRect.isValid())
        t.translate(-sourceRect.x(), -sourceRect.y());
    if (targetRect.isValid())
        t.translate(targetRect.x(), targetRect.y());

    if (sourceRect.isValid()) {
        t.scale(outputSize.width() / sourceRect.width(),
                outputSize.height() / sourceRect.height());
    }

    if (targetRect.isValid()) {
        t.scale(targetRect.width() / outputSize.width(),
                targetRect.height() / outputSize.height());
    }

    return t;
}

void WSGViewport::markFull()
{
    m_damageRegion = WDamageRegion(true);
}

void WSGViewport::addDamage(const WDamageRegion &damage)
{
    m_damageRegion += damage;
}

void WSGViewport::finishFrame()
{
    m_damageRegion.clear();
}

WAYLIB_SERVER_END_NAMESPACE
