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

WSGViewport::~WSGViewport()
{
    for (const auto &entry : m_attachedDatas) {
        if (entry.deleter)
            entry.deleter(entry.data);
    }
}

int WSGViewport::indexOfAttachedData(const void *owner) const
{
    for (int i = 0; i < m_attachedDatas.count(); ++i) {
        if (m_attachedDatas.at(i).owner == owner)
            return i;
    }
    return -1;
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

bool WSGViewport::isDirty() const
{
    if (m_damageRegion.isEmpty())
        return false;
    if (isFull() || !m_sourceRect.isValid())
        return true;

    bool invertible = false;
    const QMatrix4x4 fromSource = m_renderMatrix.inverted(&invertible);
    if (!invertible)
        return true;
    for (int i = 0; i < 16; ++i) {
        if (!qIsFinite(fromSource.constData()[i]))
            return true;
    }

    const QRectF sceneRectF = fromSource.mapRect(m_sourceRect);
    if (!qIsFinite(sceneRectF.left()) || !qIsFinite(sceneRectF.top())
        || !qIsFinite(sceneRectF.right()) || !qIsFinite(sceneRectF.bottom()))
        return true;
    return !(m_damageRegion.region & outerAligned(sceneRectF)).isEmpty();
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
