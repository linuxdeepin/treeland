// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfaceproxy.h"

#include "core/qmlengine.h"
#include "surface/surfacewrapper.h"

#include <private/qquickitem_p.h>

SurfaceProxy::SurfaceProxy(QQuickItem *parent)
    : QQuickItem(parent)
{
}

SurfaceWrapper *SurfaceProxy::surface() const
{
    return m_sourceSurface;
}

void SurfaceProxy::setSurface(SurfaceWrapper *newSurface)
{
    if (m_sourceSurface == newSurface)
        return;

    for (const QMetaObject::Connection &connection : std::as_const(m_sourceConnections)) {
        QObject::disconnect(connection);
    }
    m_sourceConnections.clear();

    m_sourceSurface = newSurface;
    if (m_proxySurface) {
        m_proxySurface->deleteLater();
        m_proxySurface = nullptr;
    }

    if (m_sourceSurface) {
        m_proxySurface = new SurfaceWrapper(m_sourceSurface,
                                            this);
        m_proxySurface->setTransformOrigin(QQuickItem::TransformOrigin::TopLeft);
        m_proxySurface->setFlag(ItemIsFocusScope);
        m_proxySurface->QQuickItem::setFocus(false);
        QQuickItemPrivate::get(m_proxySurface)->culled = true;
        if (!m_fullProxy) {
            if (!m_shadow)
                m_shadow = m_sourceSurface->m_engine->createXdgShadow(this);
            m_shadow->setProperty("cornerRadius", radius());
            m_shadow->stackBefore(m_proxySurface);
            QQuickItemPrivate::get(m_shadow)->culled = true;
        }

        auto updateProxyFlagsAndDelegate = [this]() {
            auto proxyItem = m_proxySurface ? m_proxySurface->surfaceItem() : nullptr;
            auto sourceItem = m_sourceSurface ? m_sourceSurface->surfaceItem() : nullptr;
            if (!proxyItem)
                return;

            const auto flags = m_live ? WSurfaceItem::RejectEvent
                                      : (WSurfaceItem::RejectEvent | WSurfaceItem::NonLive);
            proxyItem->setFlags(flags);

            if (!sourceItem)
                return;
            proxyItem->setDelegate(sourceItem->delegate());
        };

        updateProxyFlagsAndDelegate();

        m_sourceConnections << connect(m_proxySurface,
                                       &SurfaceWrapper::surfaceItemCreated,
                                       this,
                                       updateProxyFlagsAndDelegate);

        if (auto sourceItem = m_sourceSurface->surfaceItem()) {
            m_sourceConnections << connect(sourceItem,
                                           &WSurfaceItem::delegateChanged,
                                           this,
                                           updateProxyFlagsAndDelegate);
        } else {
            m_sourceConnections << connect(m_sourceSurface,
                                           &SurfaceWrapper::surfaceItemCreated,
                                           this,
                                           [this, updateProxyFlagsAndDelegate]() {
                                               if (auto srcItem = m_sourceSurface->surfaceItem()) {
                                                   updateProxyFlagsAndDelegate();
                                                   m_sourceConnections << connect(srcItem,
                                                                                  &WSurfaceItem::delegateChanged,
                                                                                  this,
                                                                                  updateProxyFlagsAndDelegate);
                                               }
                                           });
        }
        m_sourceConnections << connect(m_sourceSurface, &SurfaceWrapper::destroyed, this, [this] {
            Q_ASSERT(m_proxySurface);
            setSurface(nullptr);
        });
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::noTitleBarChanged,
                                       this,
                                       &SurfaceProxy::updateNoTitleBar);
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::radiusChanged,
                                       this,
                                       &SurfaceProxy::onSourceRadiusChanged);
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::noDecorationChanged,
                                       this,
                                       &SurfaceProxy::updateNoDecoration);
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::noCornerRadiusChanged,
                                       this,
                                       &SurfaceProxy::updateNoCornerRadius);

        m_sourceConnections << connect(m_proxySurface,
                                       &SurfaceWrapper::widthChanged,
                                       this,
                                       &SurfaceProxy::updateImplicitSize);
        m_sourceConnections << connect(m_proxySurface,
                                       &SurfaceWrapper::heightChanged,
                                       this,
                                       &SurfaceProxy::updateImplicitSize);

        updateImplicitSize();
        updateProxySurfaceScale();
        updateShape();
    } else {
        if (m_shadow) {
            m_shadow->deleteLater();
            m_shadow = nullptr;
        }
    }

    Q_EMIT surfaceChanged();
}

void SurfaceProxy::geometryChange(const QRectF &newGeo, const QRectF &oldGeo)
{
    QQuickItem::geometryChange(newGeo, oldGeo);

    if (m_proxySurface) {
        updateProxySurfaceScale();
        if (m_shadow)
            m_shadow->setSize(newGeo.size());
    }
}

void SurfaceProxy::updateProxySurfaceScale()
{
    if (size().isEmpty())
        return;

    QSizeF surfaceSize = m_proxySurface->size();
    surfaceSize.scale(size(), Qt::KeepAspectRatio);

    if (surfaceSize.width() < m_proxySurface->width()) {
        m_proxySurface->setScale(surfaceSize.width() / m_proxySurface->width());
        m_proxySurface->setRadius(radius() / m_proxySurface->scale());
    } else {
        m_proxySurface->setScale(1.0);
        m_proxySurface->setRadius(radius());
    }
}

void SurfaceProxy::updateShape()
{
    updateNoTitleBar();
    updateNoDecoration();
    updateNoCornerRadius();
}

void SurfaceProxy::updateNoTitleBar()
{
    m_proxySurface->setNoTitleBar(m_sourceSurface->noTitleBar());
}

void SurfaceProxy::updateNoDecoration()
{
    m_proxySurface->setNoDecoration(m_fullProxy ? m_sourceSurface->noDecoration() : false);
}

void SurfaceProxy::updateNoCornerRadius()
{
    m_proxySurface->setNoCornerRadius(m_fullProxy ? m_sourceSurface->noCornerRadius() : false);
}

void SurfaceProxy::updateImplicitSize()
{
    if (!m_proxySurface) {
        return;
    }

    const auto size = m_proxySurface->size();
    if (size.isEmpty()) {
        return;
    }

    const auto scaledSize = size.scaled(m_maxSize, Qt::KeepAspectRatio);
    const auto scale = scaledSize.width() / size.width();
    setImplicitSize(size.width() * scale, size.height() * scale);
}

void SurfaceProxy::onSourceRadiusChanged()
{
    m_proxySurface->setRadius(radius() / m_proxySurface->scale());
    if (m_shadow)
        m_shadow->setProperty("radius", radius());
    if (m_radius < 0)
        Q_EMIT radiusChanged();
}

qreal SurfaceProxy::radius() const
{
    if (m_radius >= 0)
        return m_radius;
    return m_sourceSurface ? m_sourceSurface->radius() : 0;
}

void SurfaceProxy::setRadius(qreal newRadius)
{
    if (qFuzzyCompare(m_radius, newRadius))
        return;
    m_radius = newRadius;
    if (m_proxySurface) {
        m_proxySurface->setRadius(radius() / m_proxySurface->scale());
        if (m_shadow)
            m_shadow->setProperty("radius", radius());
    }

    Q_EMIT radiusChanged();
}

void SurfaceProxy::resetRadius()
{
    setRadius(-1);
}

bool SurfaceProxy::live() const
{
    return m_live;
}

void SurfaceProxy::setLive(bool newLive)
{
    if (m_live == newLive)
        return;
    m_live = newLive;
    if (m_proxySurface) {
        auto item = m_proxySurface->surfaceItem();
        if (m_live) {
            item->setFlags(item->flags() & ~WSurfaceItem::NonLive);
        } else {
            item->setFlags(item->flags() | WSurfaceItem::NonLive);
        }
    }

    Q_EMIT liveChanged();
}

QSizeF SurfaceProxy::maxSize() const
{
    return m_maxSize;
}

void SurfaceProxy::setMaxSize(const QSizeF &newMaxSize)
{
    if (m_maxSize == newMaxSize)
        return;
    m_maxSize = newMaxSize;
    updateImplicitSize();

    Q_EMIT maxSizeChanged();
}

bool SurfaceProxy::fullProxy() const
{
    return m_fullProxy;
}

void SurfaceProxy::setFullProxy(bool newFullProxy)
{
    if (m_fullProxy == newFullProxy)
        return;
    m_fullProxy = newFullProxy;

    if (m_proxySurface) {
        if (m_fullProxy) {
            if (m_shadow) {
                m_shadow->deleteLater();
                m_shadow = nullptr;
            }
        } else if (!m_shadow) {
            m_shadow = m_sourceSurface->m_engine->createXdgShadow(this);
            m_shadow->setProperty("cornerRadius", radius());
            m_shadow->stackBefore(m_proxySurface);
            QQuickItemPrivate::get(m_shadow)->culled = true;
        }
        updateShape();
    }

    Q_EMIT fullProxyChanged();
}
