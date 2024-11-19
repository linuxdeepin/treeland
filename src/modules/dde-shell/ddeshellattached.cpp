#include "ddeshellattached.h"
#include "ddeshellmanagerinterfacev1.h"

#include <QTimer>

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

    connect(timer, &QTimer::timeout, this, [this] {
        QRectF rect{ m_target->x(), m_target->y(), m_target->width(), m_target->height() };
        region -= m_lastRect;
        m_lastRect = rect.toRect();
        region += m_lastRect;
        WindowOverlapCheckerInterface::checkRegionalConflict(region);
    });

    auto update = [timer] {
        if (!timer->isActive()) {
            timer->start();
        }
    };

    connect(target, &QQuickItem::xChanged, update);
    connect(target, &QQuickItem::yChanged, update);
    connect(target, &QQuickItem::heightChanged, update);
    connect(target, &QQuickItem::widthChanged, update);
    connect(target, &QQuickItem::destroyed, this, [this] {
        region -= m_lastRect;
        WindowOverlapCheckerInterface::checkRegionalConflict(region);
    });

    timer->start();
}

WindowOverlapChecker::~WindowOverlapChecker()
{
region -= m_lastRect;
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
