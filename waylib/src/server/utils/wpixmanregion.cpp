// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpixmanregion.h"

#include <QVarLengthArray>

#include <cstdlib>
#include <limits>
#include <utility>

WAYLIB_SERVER_BEGIN_NAMESPACE

static void requireRegion(pixman_bool_t ok)
{
    if (Q_UNLIKELY(!ok))
        std::abort();
}

WPixmanRegion::WPixmanRegion()
{
    pixman_region32_init(&m_region);
}

WPixmanRegion::WPixmanRegion(int x, int y, int width, int height)
{
    if (width > 0 && height > 0)
        pixman_region32_init_rect(&m_region, x, y, unsigned(width), unsigned(height));
    else
        pixman_region32_init(&m_region);
}

WPixmanRegion::WPixmanRegion(const QRect &rect)
    : WPixmanRegion(rect.x(), rect.y(), rect.width(), rect.height())
{
}

WPixmanRegion::WPixmanRegion(const pixman_region32_t *region)
{
    pixman_region32_init(&m_region);
    if (region)
        requireRegion(pixman_region32_copy(&m_region, region));
}

WPixmanRegion::WPixmanRegion(const WPixmanRegion &other)
    : WPixmanRegion(other.native())
{
}

WPixmanRegion::WPixmanRegion(WPixmanRegion &&other) noexcept
    : m_region(other.m_region)
{
    pixman_region32_init(&other.m_region);
}

WPixmanRegion::~WPixmanRegion()
{
    pixman_region32_fini(&m_region);
}

WPixmanRegion &WPixmanRegion::operator=(const WPixmanRegion &other)
{
    if (this != &other)
        requireRegion(pixman_region32_copy(&m_region, &other.m_region));
    return *this;
}

WPixmanRegion &WPixmanRegion::operator=(WPixmanRegion &&other) noexcept
{
    if (this == &other)
        return *this;
    pixman_region32_fini(&m_region);
    m_region = other.m_region;
    pixman_region32_init(&other.m_region);
    return *this;
}

bool WPixmanRegion::isEmpty() const
{
    return !pixman_region32_not_empty(&m_region);
}

int WPixmanRegion::rectCount() const
{
    return pixman_region32_n_rects(&m_region);
}

QRect WPixmanRegion::boundingRect() const
{
    if (isEmpty())
        return { };
    const pixman_box32_t *box = pixman_region32_extents(&m_region);
    return QRect(box->x1, box->y1, box->x2 - box->x1, box->y2 - box->y1);
}

const pixman_box32_t *WPixmanRegion::rectangles(int *count) const
{
    return pixman_region32_rectangles(&m_region, count);
}

void WPixmanRegion::clear()
{
    pixman_region32_clear(&m_region);
}

void WPixmanRegion::translate(int dx, int dy)
{
    pixman_region32_translate(&m_region, dx, dy);
}

WPixmanRegion WPixmanRegion::translated(int dx, int dy) const
{
    WPixmanRegion result(*this);
    result.translate(dx, dy);
    return result;
}

void WPixmanRegion::setRects(const pixman_box32_t *rects, int count)
{
    pixman_region32_fini(&m_region);
    if (count > 0)
        requireRegion(pixman_region32_init_rects(&m_region, rects, count));
    else
        pixman_region32_init(&m_region);
}

WPixmanRegion WPixmanRegion::fromRects(const pixman_box32_t *rects, int count)
{
    WPixmanRegion result;
    result.setRects(rects, count);
    return result;
}

void WPixmanRegion::setIntersection(const pixman_region32_t *source, const QRect &rect)
{
    if (!source || rect.isEmpty()) {
        clear();
        return;
    }
    requireRegion(pixman_region32_intersect_rect(&m_region,
                                                 source,
                                                 rect.x(),
                                                 rect.y(),
                                                 unsigned(rect.width()),
                                                 unsigned(rect.height())));
}

void WPixmanRegion::setIntersection(const pixman_region32_t *lhs, const pixman_region32_t *rhs)
{
    if (!lhs || !rhs) {
        clear();
        return;
    }
    requireRegion(pixman_region32_intersect(&m_region, lhs, rhs));
}

WPixmanRegion &WPixmanRegion::operator+=(const WPixmanRegion &other)
{
    return *this += other.native();
}

WPixmanRegion &WPixmanRegion::operator+=(const pixman_region32_t *other)
{
    if (other && pixman_region32_not_empty(other))
        requireRegion(pixman_region32_union(&m_region, &m_region, other));
    return *this;
}

WPixmanRegion &WPixmanRegion::operator+=(const QRect &rect)
{
    if (!rect.isEmpty()) {
        requireRegion(pixman_region32_union_rect(&m_region,
                                                 &m_region,
                                                 rect.x(),
                                                 rect.y(),
                                                 unsigned(rect.width()),
                                                 unsigned(rect.height())));
    }
    return *this;
}

WPixmanRegion &WPixmanRegion::operator-=(const WPixmanRegion &other)
{
    return *this -= other.native();
}

WPixmanRegion &WPixmanRegion::operator-=(const pixman_region32_t *other)
{
    if (other && pixman_region32_not_empty(other))
        requireRegion(pixman_region32_subtract(&m_region, &m_region, other));
    return *this;
}

WPixmanRegion &WPixmanRegion::operator-=(const QRect &rect)
{
    return *this -= WPixmanRegion(rect);
}

WPixmanRegion &WPixmanRegion::operator&=(const WPixmanRegion &other)
{
    requireRegion(pixman_region32_intersect(&m_region, &m_region, &other.m_region));
    return *this;
}

WPixmanRegion &WPixmanRegion::operator&=(const QRect &rect)
{
    if (rect.isEmpty()) {
        clear();
    } else {
        requireRegion(pixman_region32_intersect_rect(&m_region,
                                                     &m_region,
                                                     rect.x(),
                                                     rect.y(),
                                                     unsigned(rect.width()),
                                                     unsigned(rect.height())));
    }
    return *this;
}

bool WPixmanRegion::operator==(const WPixmanRegion &other) const
{
    return pixman_region32_equal(&m_region, &other.m_region);
}

WPixmanRegion operator+(WPixmanRegion lhs, const WPixmanRegion &rhs)
{
    lhs += rhs;
    return lhs;
}

WPixmanRegion operator+(WPixmanRegion lhs, const QRect &rhs)
{
    lhs += rhs;
    return lhs;
}

WPixmanRegion operator-(WPixmanRegion lhs, const WPixmanRegion &rhs)
{
    lhs -= rhs;
    return lhs;
}

WPixmanRegion operator-(WPixmanRegion lhs, const QRect &rhs)
{
    lhs -= rhs;
    return lhs;
}

WPixmanRegion operator&(WPixmanRegion lhs, const WPixmanRegion &rhs)
{
    lhs &= rhs;
    return lhs;
}

WPixmanRegion operator&(WPixmanRegion lhs, const QRect &rhs)
{
    lhs &= rhs;
    return lhs;
}

static pixman_box32_t boxFromRect(const QRect &rect)
{
    return {
        rect.x(),
        rect.y(),
        rect.x() + rect.width(),
        rect.y() + rect.height(),
    };
}

struct ActiveBox
{
    int x1;
    int x2;
    int resultIndex;
};

static QVarLengthArray<pixman_box32_t, 32> maximalRectangles(const WPixmanRegion &region)
{
    int count = 0;
    const pixman_box32_t *boxes = region.rectangles(&count);
    QVarLengthArray<pixman_box32_t, 32> result;
    result.reserve(count);
    QVarLengthArray<ActiveBox, 32> active;

    for (int bandStart = 0; bandStart < count;) {
        int bandEnd = bandStart + 1;
        while (bandEnd < count && boxes[bandEnd].y1 == boxes[bandStart].y1
               && boxes[bandEnd].y2 == boxes[bandStart].y2) {
            ++bandEnd;
        }

        QVarLengthArray<ActiveBox, 32> next;
        next.reserve(bandEnd - bandStart);
        int activeIndex = 0;
        for (int i = bandStart; i < bandEnd; ++i) {
            const pixman_box32_t &box = boxes[i];
            while (activeIndex < active.size()
                   && (active[activeIndex].x1 < box.x1
                       || (active[activeIndex].x1 == box.x1 && active[activeIndex].x2 < box.x2))) {
                ++activeIndex;
            }

            int resultIndex = -1;
            if (activeIndex < active.size() && active[activeIndex].x1 == box.x1
                && active[activeIndex].x2 == box.x2
                && result[active[activeIndex].resultIndex].y2 == box.y1) {
                resultIndex = active[activeIndex].resultIndex;
                result[resultIndex].y2 = box.y2;
                ++activeIndex;
            } else {
                resultIndex = result.size();
                result.append(box);
            }
            next.append({ box.x1, box.x2, resultIndex });
        }

        active = std::move(next);
        bandStart = bandEnd;
    }

    return result;
}

WPixmanRegion WPixmanRegion::mappedOuter(const QTransform &transform) const
{
    if (isEmpty())
        return { };
    if (transform.isIdentity())
        return *this;
    if (transform.type() <= QTransform::TxTranslate) {
        const qreal dx = transform.dx();
        const qreal dy = transform.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return translated(int(dx), int(dy));
    }
    if (!isAxisAligned(transform)) {
        const QVarLengthArray<pixman_box32_t, 32> boxes = maximalRectangles(*this);
        QVarLengthArray<pixman_box32_t, 32> mapped;
        mapped.reserve(boxes.size());
        for (const pixman_box32_t &box : boxes) {
            const QRect rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            const QRect result = mapOuter(transform, QRectF(rect));
            if (!result.isEmpty())
                mapped.append(boxFromRect(result));
        }
        return fromRects(mapped.constData(), mapped.size());
    }

    int count = 0;
    const pixman_box32_t *boxes = rectangles(&count);
    QVarLengthArray<pixman_box32_t, 32> mapped;
    mapped.reserve(count);
    for (int i = 0; i < count; ++i) {
        const QRect rect(boxes[i].x1,
                         boxes[i].y1,
                         boxes[i].x2 - boxes[i].x1,
                         boxes[i].y2 - boxes[i].y1);
        const QRect result = mapOuter(transform, QRectF(rect));
        if (!result.isEmpty())
            mapped.append(boxFromRect(result));
    }

    WPixmanRegion output;
    output.setRects(mapped.constData(), mapped.size());
    return output;
}

WPixmanRegion WPixmanRegion::mappedInner(const QTransform &transform) const
{
    if (isEmpty() || !isAxisAligned(transform))
        return { };
    if (transform.isIdentity())
        return *this;
    if (transform.type() <= QTransform::TxTranslate) {
        const qreal dx = transform.dx();
        const qreal dy = transform.dy();
        if (std::floor(dx) == dx && std::floor(dy) == dy)
            return translated(int(dx), int(dy));
    }
    const QVarLengthArray<pixman_box32_t, 32> boxes = maximalRectangles(*this);
    WPixmanRegion output;
    for (const pixman_box32_t &box : boxes) {
        const QRect rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
        output += mapInner(transform, QRectF(rect));
    }
    return output;
}

WPixmanRegion WPixmanRegion::mappedOuter(const QMatrix4x4 &matrix) const
{
    return mappedOuter(matrix.toTransform());
}

WPixmanRegion WPixmanRegion::mappedInner(const QMatrix4x4 &matrix) const
{
    return mappedInner(matrix.toTransform());
}

WPixmanRegion dilateRegion(const WPixmanRegion &region, const QMargins &margins)
{
    if (region.isEmpty())
        return { };
    if (margins.isNull())
        return region;

    int count = 0;
    const pixman_box32_t *boxes = region.rectangles(&count);
    QVarLengthArray<pixman_box32_t, 32> expanded;
    expanded.reserve(count);
    for (int i = 0; i < count; ++i) {
        const qint64 x = qint64(boxes[i].x1) - margins.left();
        const qint64 y = qint64(boxes[i].y1) - margins.top();
        const qint64 w = qint64(boxes[i].x2) - boxes[i].x1 + margins.left() + margins.right();
        const qint64 h = qint64(boxes[i].y2) - boxes[i].y1 + margins.top() + margins.bottom();
        const int x1 = int(qBound(qint64(std::numeric_limits<int>::min()),
                                  x,
                                  qint64(std::numeric_limits<int>::max())));
        const int y1 = int(qBound(qint64(std::numeric_limits<int>::min()),
                                  y,
                                  qint64(std::numeric_limits<int>::max())));
        const int width = int(qBound(qint64(0), w, qint64(std::numeric_limits<int>::max())));
        const int height = int(qBound(qint64(0), h, qint64(std::numeric_limits<int>::max())));
        if (width > 0 && height > 0)
            expanded.append(boxFromRect(QRect(x1, y1, width, height)));
    }
    return WPixmanRegion::fromRects(expanded.constData(), expanded.size());
}

QRegion WPixmanRegion::toQRegion() const
{
    return toQRegion(&m_region);
}

QRegion WPixmanRegion::toQRegion(const pixman_region32_t *region)
{
    QRegion result;
    if (!region)
        return result;
    int count = 0;
    const pixman_box32_t *boxes = pixman_region32_rectangles(region, &count);
    QVarLengthArray<QRect, 32> rects;
    rects.reserve(count);
    for (int i = 0; i < count; ++i) {
        rects.append(
            QRect(boxes[i].x1, boxes[i].y1, boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1));
    }
    result.setRects(rects.constData(), rects.size());
    return result;
}

WPixmanRegion WPixmanRegion::fromQRegion(const QRegion &region)
{
    QVarLengthArray<pixman_box32_t, 32> boxes;
    boxes.reserve(region.rectCount());
    for (const QRect &rect : region)
        boxes.append(boxFromRect(rect));
    WPixmanRegion result;
    result.setRects(boxes.constData(), boxes.size());
    return result;
}

WAYLIB_SERVER_END_NAMESPACE
