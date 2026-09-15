// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellattached.h"

#include "ddeshellmanagerinterfacev1.h"

#include <QTimer>

#include <utility>

DDEShellAttached::DDEShellAttached(QQuickItem *target, QObject *parent)
    : QObject(parent)
    , m_target(target)
{
}

WindowOverlapChecker::WindowOverlapChecker(QQuickItem *target, QObject *parent)
    : DDEShellAttached(target, parent)
{
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(300);

    connect(timer, &QTimer::timeout, this, &WindowOverlapChecker::updateOverlapRegion);

    auto update = [timer] {
        if (!timer->isActive()) {
            timer->start();
        }
    };

    connect(target, &QQuickItem::xChanged, update);
    connect(target, &QQuickItem::yChanged, update);
    connect(target, &QQuickItem::heightChanged, update);
    connect(target, &QQuickItem::widthChanged, update);
    connect(target, &QQuickItem::visibleChanged, update);
    connect(target, &QQuickItem::destroyed, this, [this] {
        if (s_checkers.removeOne(this)) {
            notifyOverlapChange();
        }
    });

    s_checkers.append(this);
    timer->start();
}

WindowOverlapChecker::~WindowOverlapChecker()
{
    if (s_checkers.removeOne(this)) {
        notifyOverlapChange();
    }
}

void WindowOverlapChecker::updateOverlapRegion()
{
    m_rect = m_target->isVisible()
        ? QRectF{ m_target->x(), m_target->y(), m_target->width(), m_target->height() }.toRect()
        : QRect();

    notifyOverlapChange();
}

void WindowOverlapChecker::notifyOverlapChange()
{
    QList<QRect> windowRects;
    windowRects.reserve(s_checkers.size());
    for (const WindowOverlapChecker *checker : std::as_const(s_checkers)) {
        if (!checker->m_rect.isEmpty()) {
            windowRects.append(checker->m_rect);
        }
    }

    WindowOverlapCheckerInterface::checkRegionalConflict(windowRects);
}

void WindowOverlapChecker::setOverlapped(bool overlapped)
{
    if (m_overlapped == overlapped) {
        return;
    }

    m_overlapped = overlapped;
    Q_EMIT overlappedChanged();
}

DDEShellAttached *DDEShellHelper::qmlAttachedProperties(QObject *target)
{
    if (auto *item = qobject_cast<QQuickItem *>(target)) {
        return new WindowOverlapChecker(item);
    }

    return nullptr;
}
