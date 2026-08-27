// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wpixmanregion.h>

#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QTransform>
WAYLIB_SERVER_BEGIN_NAMESPACE

// Per-render-target mapping plus that target's accumulated scene damage.
// WSGDamageTracker::commit(WSGViewport&) copies this round's scene damage
// into every viewport; each viewport keeps it until it presents, so outputs
// sharing one scene at different refresh rates never lose damage.
// Mapping changes (matrix / source / target / DPR) mark full: the whole
// buffer contents are invalid. Size changes are WBufferRenderer's job.
// Damage stays in scene coordinates; whether it hits this output is decided
// with the current mapping and the buffer pixel size at draw time.
class WAYLIB_SERVER_EXPORT WSGViewport
{
public:
    WSGViewport();

    void setDevicePixelRatio(qreal devicePixelRatio);
    void setRenderParameters(const QMatrix4x4 &renderMatrix,
                             const QRectF &sourceRect = {},
                             const QRectF &targetRect = {});

    qreal devicePixelRatio() const;
    const QMatrix4x4 &renderMatrix() const;
    QRectF sourceRect() const;
    QRectF targetRect() const;

    QMatrix4x4 sceneToBufferTransform(const QSize &pixelSize) const;
    // Scene-space coverage of a buffer with origin (0, 0). Empty when the
    // mapping has no finite inverse or pixelSize is empty.
    QRect sceneOutputRect(const QSize &pixelSize) const;
    // True if accumulated scene damage hits this buffer under the current
    // mapping. Unmappable viewports conservatively return true.
    bool affectsBuffer(const QSize &pixelSize) const;

    static QTransform inputMapToOutput(const QRectF &sourceRect, const QRectF &targetRect,
                                      const QSize &pixelSize, const qreal devicePixelRatio);

    const WDamageRegion &damageRegion() const
    {
        return m_damageRegion;
    }

    bool isFull() const
    {
        return m_damageRegion.isFull;
    }

    void markFull();
    void addDamage(const WDamageRegion &damage);
    void finishFrame();

private:
    qreal m_devicePixelRatio = 1.0;
    QMatrix4x4 m_renderMatrix;
    QRectF m_sourceRect;
    QRectF m_targetRect;
    WDamageRegion m_damageRegion;
};

WAYLIB_SERVER_END_NAMESPACE
