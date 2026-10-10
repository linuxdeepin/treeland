// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <pixman.h>

#include <wglobal.h>

#include <QMargins>
#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QDebug>
#include <QRegion>
#include <QTransform>
#include <QtCore/qglobal.h>

#include <cmath>
#include <limits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WPixmanRegion
{
public:
    WPixmanRegion();
    WPixmanRegion(int x, int y, int width, int height);
    explicit WPixmanRegion(const QRect &rect);
    explicit WPixmanRegion(const pixman_region32_t *region);
    WPixmanRegion(const WPixmanRegion &other);
    WPixmanRegion(WPixmanRegion &&other) noexcept;
    ~WPixmanRegion();

    WPixmanRegion &operator=(const WPixmanRegion &other);
    WPixmanRegion &operator=(WPixmanRegion &&other) noexcept;

    const pixman_region32_t *native() const
    {
        return &m_region;
    }

    pixman_region32_t *native()
    {
        return &m_region;
    }

    pixman_region32_t *get()
    {
        return native();
    }

    operator pixman_region32_t *()
    {
        return native();
    }

    operator const pixman_region32_t *() const
    {
        return native();
    }

    bool isEmpty() const;
    bool intersects(const QRect &rect) const;
    int rectCount() const;
    QRect boundingRect() const;
    const pixman_box32_t *rectangles(int *count) const;

    void clear();
    void translate(int dx, int dy);
    WPixmanRegion translated(int dx, int dy) const;
    // Regions contain integer, axis-aligned rectangles. Outer mapping
    // conservatively covers every transformed pixel (damage); inner mapping
    // contains only guaranteed covered pixels (opaque).
    WPixmanRegion mappedOuter(const QTransform &transform) const;
    WPixmanRegion mappedInner(const QTransform &transform) const;
    WPixmanRegion mappedOuter(const QMatrix4x4 &matrix) const;
    WPixmanRegion mappedInner(const QMatrix4x4 &matrix) const;
    static WPixmanRegion fromRects(const pixman_box32_t *rects, int count);
    void setIntersection(const pixman_region32_t *source, const QRect &rect);
    void setIntersection(const pixman_region32_t *lhs, const pixman_region32_t *rhs);

    WPixmanRegion &operator+=(const WPixmanRegion &other);
    WPixmanRegion &operator+=(const pixman_region32_t *other);
    WPixmanRegion &operator+=(const QRect &rect);
    WPixmanRegion &operator-=(const WPixmanRegion &other);
    WPixmanRegion &operator-=(const pixman_region32_t *other);
    WPixmanRegion &operator-=(const QRect &rect);
    WPixmanRegion &operator&=(const WPixmanRegion &other);
    WPixmanRegion &operator&=(const QRect &rect);

    bool operator==(const WPixmanRegion &other) const;

    bool operator!=(const WPixmanRegion &other) const
    {
        return !(*this == other);
    }

    QRegion toQRegion() const;
    static QRegion toQRegion(const pixman_region32_t *region);
    static WPixmanRegion fromQRegion(const QRegion &region);

private:
    void setRects(const pixman_box32_t *rects, int count);
    pixman_region32_t m_region;
};

struct WAYLIB_SERVER_EXPORT WDamageRegion
{
    WPixmanRegion region;
    bool isFull = false;

    WDamageRegion() = default;
    WDamageRegion(const WPixmanRegion &r, bool full = false)
        : region(r), isFull(full) {}
    WDamageRegion(WPixmanRegion &&r, bool full = false)
        : region(std::move(r)), isFull(full) {}
    explicit WDamageRegion(bool full)
        : isFull(full) {}

    bool isEmpty() const { return !isFull && region.isEmpty(); }
    void clear() { region.clear(); isFull = false; }

    WDamageRegion &operator+=(const WDamageRegion &other)
    {
        if (other.isFull) {
            isFull = true;
            region.clear();
        } else if (!isFull) {
            region += other.region;
        }
        return *this;
    }

    bool operator==(const WDamageRegion &other) const
    {
        return isFull == other.isFull && region == other.region;
    }

    bool operator!=(const WDamageRegion &other) const
    {
        return !(*this == other);
    }
};

WPixmanRegion operator+(WPixmanRegion lhs, const WPixmanRegion &rhs);
WPixmanRegion operator+(WPixmanRegion lhs, const QRect &rhs);
WPixmanRegion operator-(WPixmanRegion lhs, const WPixmanRegion &rhs);
WPixmanRegion operator-(WPixmanRegion lhs, const QRect &rhs);
WPixmanRegion operator&(WPixmanRegion lhs, const WPixmanRegion &rhs);

#ifndef QT_NO_DEBUG_STREAM
inline QDebug operator<<(QDebug debug, const WPixmanRegion &region)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "WPixmanRegion(";
    int count = 0;
    const pixman_box32_t *rects = region.rectangles(&count);
    if (!rects || count == 0) {
        debug << "empty)";
        return debug;
    }
    debug << count << " rects:";
    for (int i = 0; i < count; ++i) {
        debug << ' ' << (rects[i].x2 - rects[i].x1) << 'x' << (rects[i].y2 - rects[i].y1)
              << '+' << rects[i].x1 << '+' << rects[i].y1;
    }
    debug << ')';
    return debug;
}
#endif
WPixmanRegion operator&(WPixmanRegion lhs, const QRect &rhs);

inline int clampToIntCoord(double v)
{
    constexpr double lo = double(std::numeric_limits<int>::min());
    constexpr double hi = double(std::numeric_limits<int>::max());
    if (v <= lo)
        return std::numeric_limits<int>::min();
    if (v >= hi)
        return std::numeric_limits<int>::max();
    return int(v);
}

// Pixel coverage of a continuous rect: over-estimate (damage) and under-estimate (opaque).
inline QRect outerAligned(const QRectF &r)
{
    if (Q_UNLIKELY(!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0))
        return { };
    const int x1 = clampToIntCoord(std::floor(r.left()));
    const int y1 = clampToIntCoord(std::floor(r.top()));
    const int x2 = clampToIntCoord(std::ceil(r.right()));
    const int y2 = clampToIntCoord(std::ceil(r.bottom()));
    if (Q_UNLIKELY(x2 <= x1 || y2 <= y1))
        return { };
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

inline QRect innerAligned(const QRectF &r)
{
    if (Q_UNLIKELY(!r.isValid() || r.width() <= 0.0 || r.height() <= 0.0))
        return { };
    const int x1 = clampToIntCoord(std::ceil(r.left()));
    const int y1 = clampToIntCoord(std::ceil(r.top()));
    const int x2 = clampToIntCoord(std::floor(r.right()));
    const int y2 = clampToIntCoord(std::floor(r.bottom()));
    if (Q_UNLIKELY(x2 <= x1 || y2 <= y1))
        return { };
    return QRect(x1, y1, x2 - x1, y2 - y1);
}

// Axis-aligned = translation / scale / 90-degree rotation. No shear/arbitrary rotation.
inline bool isAxisAligned(const QTransform &t)
{
    if (Q_UNLIKELY(!t.isAffine()))
        return false;
    const bool noOffAxis = qFuzzyIsNull(t.m12()) && qFuzzyIsNull(t.m21());
    const bool rot90 = qFuzzyIsNull(t.m11()) && qFuzzyIsNull(t.m22());
    return noOffAxis || rot90;
}

inline QRect mapOuter(const QTransform &t, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return { };
    if (Q_LIKELY(t.isIdentity()))
        return outerAligned(local);
    return outerAligned(t.mapRect(local));
}

inline QRect mapOuter(const QMatrix4x4 &matrix, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return { };
    if (matrix.isIdentity())
        return outerAligned(local);
    const float xs[2] = { float(local.left()), float(local.right()) };
    const float ys[2] = { float(local.top()), float(local.bottom()) };
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool first = true;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            const QVector3D p = matrix.map(QVector3D(xs[i], ys[j], 0.0f));
            if (first) {
                x0 = x1 = p.x();
                y0 = y1 = p.y();
                first = false;
            } else {
                x0 = qMin(x0, p.x());
                y0 = qMin(y0, p.y());
                x1 = qMax(x1, p.x());
                y1 = qMax(y1, p.y());
            }
        }
    }
    return outerAligned(QRectF(x0, y0, x1 - x0, y1 - y0));
}

inline QRect mapInner(const QTransform &t, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return { };
    if (Q_UNLIKELY(!isAxisAligned(t)))
        return { };
    if (Q_LIKELY(t.isIdentity()))
        return innerAligned(local);
    return innerAligned(t.mapRect(local));
}

// Pixels guaranteed inside a rounded rect, as axis-aligned boxes. The four
// corner squares of size ceil(radius) are omitted so quarter-circles are
// never claimed as opaque. Damage/clip bounds should still use the AABB.
inline WPixmanRegion roundedRectInnerRegion(const QRectF &rect, qreal radius)
{
    const QRect box = innerAligned(rect);
    if (box.isEmpty())
        return { };
    qreal clamped = radius;
    if (clamped < 0)
        clamped = 0;
    const qreal halfW = rect.width() * 0.5;
    const qreal halfH = rect.height() * 0.5;
    if (clamped > halfW)
        clamped = halfW;
    if (clamped > halfH)
        clamped = halfH;
    if (clamped <= 0)
        return WPixmanRegion(box);
    const int r = int(std::ceil(clamped));
    if (r <= 0)
        return WPixmanRegion(box);
    WPixmanRegion region(box);
    region -= QRect(box.x(), box.y(), r, r);
    region -= QRect(box.x() + box.width() - r, box.y(), r, r);
    region -= QRect(box.x(), box.y() + box.height() - r, r, r);
    region -= QRect(box.x() + box.width() - r, box.y() + box.height() - r, r, r);
    return region;
}

WPixmanRegion dilateRegion(const WPixmanRegion &region, const QMargins &margins);

namespace WPixman {

// QRegion overloads for Qt / wlr edges.
inline QRegion dilateRegion(const QRegion &r, const QMargins &m)
{
    if (r.isEmpty())
        return { };
    if (m.isNull())
        return r;
    if (r.rectCount() == 1)
        return QRegion(r.boundingRect().marginsAdded(m));
    QRegion out;
    for (const QRect &rect : r)
        out += rect.marginsAdded(m);
    return out;
}

} // namespace WPixman

WAYLIB_SERVER_END_NAMESPACE
