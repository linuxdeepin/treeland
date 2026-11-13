// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfaceproxy.h"

#include "core/qmlengine.h"
#include "surface/surfacewrapper.h"

#include <private/qquickitem_p.h>

namespace {
// Display-only radius for a client-side decorated window in a non-maximized state, in proxy
// coordinates. Its clip contour stays square so the drawn corner and shadow in the client's
// buffer are not cut away.
constexpr qreal kClientSideDecoratedRadius = 6;
}

SurfaceProxy::SurfaceProxy(QQuickItem *parent)
    : QQuickItem(parent)
{
}

SurfaceProxy::~SurfaceProxy()
{
    if (m_proxySurface) {
        m_proxySurface->destroy();
        m_proxySurface = nullptr;
    }
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
        m_proxySurface->destroy();
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
            m_shadow->stackBefore(m_proxySurface);
            QQuickItemPrivate::get(m_shadow)->culled = true;
        }

        m_sourceConnections << connect(m_proxySurface, &SurfaceWrapper::frameGeometryChanged,
                                       this, &SurfaceProxy::updateShadowGeometry);
        m_sourceConnections << connect(m_proxySurface, &QQuickItem::scaleChanged,
                                       this, &SurfaceProxy::updateShadowGeometry);
        m_sourceConnections << connect(m_proxySurface, &SurfaceWrapper::presentationFrameGeometryChanged,
                                       this, &SurfaceProxy::effectiveRadiusChanged);
        m_sourceConnections << connect(m_proxySurface, &QQuickItem::scaleChanged,
                                       this, &SurfaceProxy::effectiveRadiusChanged);
        auto updateProxyFlagsAndDelegate = [this]() {
            auto proxyItem = m_proxySurface ? m_proxySurface->surfaceItem() : nullptr;
            auto sourceItem = m_sourceSurface ? m_sourceSurface->surfaceItem() : nullptr;
            if (!proxyItem)
                return;

            const bool live = m_live && m_proxySurface->renderingEnabled();
            const auto flags = live ? WSurfaceItem::RejectEvent
                                    : (WSurfaceItem::RejectEvent | WSurfaceItem::NonLive);
            proxyItem->setFlags(flags);

            if (!sourceItem)
                return;
            proxyItem->setDelegate(sourceItem->delegate());
        };

        updateProxyFlagsAndDelegate();
        m_sourceConnections << connect(m_proxySurface, &SurfaceWrapper::renderingEnabledChanged,
                                       this, &SurfaceProxy::updateLive);

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
        // The proxy radius depends on the source decoration mode and state.
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::noDecorationChanged,
                                       this,
                                       &SurfaceProxy::updateProxySurfaceScale);
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::surfaceStateChanged,
                                       this,
                                       &SurfaceProxy::updateProxySurfaceScale);
        // effectiveRadius() also reads the source state (a client-side decorated proxy keeps a
        // display radius while its clip contour is square), so notify it directly.
        m_sourceConnections << connect(m_sourceSurface,
                                       &SurfaceWrapper::surfaceStateChanged,
                                       this,
                                       &SurfaceProxy::effectiveRadiusChanged);

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
        updateShadowGeometry();
    } else {
        if (m_shadow) {
            m_shadow->deleteLater();
            m_shadow = nullptr;
        }
    }

    Q_EMIT surfaceChanged();
    Q_EMIT effectiveRadiusChanged();
}

void SurfaceProxy::geometryChange(const QRectF &newGeo, const QRectF &oldGeo)
{
    QQuickItem::geometryChange(newGeo, oldGeo);

    if (m_proxySurface) {
        updateProxySurfaceScale();
        updateShadowGeometry();
    }
}

void SurfaceProxy::updateProxySurfaceScale()
{
    if (size().isEmpty())
        return;

    QSizeF surfaceSize = m_proxySurface->size();
    surfaceSize.scale(size(), Qt::KeepAspectRatio);

    if (surfaceSize.width() < m_proxySurface->width())
        m_proxySurfaceScale = surfaceSize.width() / m_proxySurface->width();
    else
        m_proxySurfaceScale = 1.0;
    m_proxySurface->setScale(m_proxySurfaceScale);
    m_proxySurface->setRadius(radius() / m_proxySurfaceScale);
    // radius() also follows the source decoration state and the proxy scale.
    Q_EMIT radiusChanged();
}

void SurfaceProxy::updateShadowGeometry()
{
    if (!m_shadow || !m_proxySurface)
        return;
    const auto geometry = m_proxySurface->frameGeometry();
    m_shadow->setSize(geometry.rect().size() * m_proxySurface->scale());
    m_shadow->setProperty("cornerRadius", geometry.radius() * m_proxySurface->scale());
}

void SurfaceProxy::updateShape()
{
    updateNoTitleBar();
    updateNoDecoration();
}

void SurfaceProxy::updateNoTitleBar()
{
    m_proxySurface->setNoTitleBar(m_sourceSurface->noTitleBar());
}

void SurfaceProxy::updateNoDecoration()
{
    m_proxySurface->setNoDecoration(m_fullProxy ? m_sourceSurface->noDecoration() : false);
    // radius() already decides whether the thumbnail needs rounded clipping.
    m_proxySurface->setNoCornerRadius(false);
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
    m_proxySurface->setRadius(radius() / m_proxySurfaceScale);
    if (m_radius < 0)
        Q_EMIT radiusChanged();
}

bool SurfaceProxy::isFullSizeState() const
{
    const auto state = m_sourceSurface->surfaceState();
    return state == SurfaceWrapper::State::Maximized
        || state == SurfaceWrapper::State::Fullscreen;
}

bool SurfaceProxy::keepsClientSideMargins() const
{
    if (!m_sourceSurface || !m_sourceSurface->noDecoration())
        return false;
    // A maximized or fullscreen client drops its CSD margins: there is no drawn corner or
    // shadow left in the buffer to protect.
    return !isFullSizeState();
}

qreal SurfaceProxy::radius() const
{
    if (m_radius >= 0)
        return m_radius;
    if (!m_sourceSurface)
        return 0;
    // A client-side decorated client draws its corners and shadow inside its own buffer,
    // outside the frame geometry, so a rounded clip would cut that shadow away.
    if (keepsClientSideMargins())
        return 0;
    // A maximized or fullscreen window has no visible corner of its own, and its window radius
    // shrunk to thumbnail scale is only a few pixels there, so the thumbnail takes the radius
    // as a display value instead of a faithful sample of the scaled window.
    if (isFullSizeState())
        return m_sourceSurface->radius();
    // A thumbnail keeps the window's corner shape: the radius shrinks with the proxy.
    return m_sourceSurface->radius() * m_proxySurfaceScale;
}

qreal SurfaceProxy::effectiveRadius() const
{
    // The outline keeps a display radius for a thumbnail whose clip contour is square.
    if (m_radius < 0 && keepsClientSideMargins())
        return kClientSideDecoratedRadius;
    return m_proxySurface
        ? m_proxySurface->presentationFrameGeometry().radius() * m_proxySurface->scale()
        : 0;
}

void SurfaceProxy::setRadius(qreal newRadius)
{
    if (qFuzzyCompare(m_radius, newRadius))
        return;
    m_radius = newRadius;
    if (m_proxySurface)
        m_proxySurface->setRadius(radius() / m_proxySurfaceScale);

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

void SurfaceProxy::updateLive()
{
    auto *item = m_proxySurface ? m_proxySurface->surfaceItem() : nullptr;
    if (!item)
        return;
    const bool live = m_live && m_proxySurface->renderingEnabled();
    item->setFlags(live ? item->flags() & ~WSurfaceItem::NonLive
                        : item->flags() | WSurfaceItem::NonLive);
}

void SurfaceProxy::setLive(bool newLive)
{
    if (m_live == newLive)
        return;
    m_live = newLive;
    updateLive();

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
            m_shadow->stackBefore(m_proxySurface);
            QQuickItemPrivate::get(m_shadow)->culled = true;
        }
        updateShape();
        updateShadowGeometry();
    }

    Q_EMIT fullProxyChanged();
}
