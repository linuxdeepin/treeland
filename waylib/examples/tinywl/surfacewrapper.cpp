// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfacewrapper.h"
#include "qmlengine.h"
#include "output.h"

#include <woutput.h>
#include <wxdgtoplevelsurfaceitem.h>
#include <wxdgpopupsurfaceitem.h>
#include <wlayersurfaceitem.h>
#include <wxwaylandsurfaceitem.h>
#include <winputpopupsurfaceitem.h>
#include <wxwaylandsurface.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>

SurfaceWrapper::SurfaceWrapper(QmlEngine *qmlEngine, WToplevelSurface *shellSurface, Type type, QQuickItem *parent)
    : QQuickItem(parent)
    , m_engine(qmlEngine)
    , m_shellSurface(shellSurface)
    , m_type(type)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_titleBarState(TitleBarState::Default)
    , m_noCornerRadius(false)
    , m_alwaysOnTop(false)
    , m_xwaylandPositionFromSurface(true)
{
    QQmlEngine::setContextForObject(this, qmlEngine->rootContext());

    switch (type) {
    case Type::XdgToplevel:
        m_surfaceItem = new WXdgToplevelSurfaceItem(this);
        break;
    case Type::XdgPopup:
        m_surfaceItem = new WXdgPopupSurfaceItem(this);
        break;
    case Type::Layer:
        m_surfaceItem = new WLayerSurfaceItem(this);
        break;
    case Type::XWayland: {
        m_surfaceItem = new WXWaylandSurfaceItem(this);
        connect(m_surfaceItem, &WSurfaceItem::bufferScaleChanged,
                this, &SurfaceWrapper::updateSurfaceSizeRatio);
        updateSurfaceSizeRatio();
        break;
    }
    case Type::InputPopup:
        m_surfaceItem = new WInputPopupSurfaceItem(this);
        break;
    default:
        Q_UNREACHABLE();
    }

    QQmlEngine::setContextForObject(m_surfaceItem, qmlEngine->rootContext());
    m_surfaceItem->setDelegate(qmlEngine->surfaceContentComponent());
    m_surfaceItem->setResizeMode(WSurfaceItem::ManualResize);
    m_surfaceItem->setShellSurface(shellSurface);

    shellSurface->safeConnect(&WToplevelSurface::requestMinimize, this, &SurfaceWrapper::requestMinimize);
    shellSurface->safeConnect(&WToplevelSurface::requestCancelMinimize, this, &SurfaceWrapper::requestCancelMinimize);
    shellSurface->safeConnect(&WToplevelSurface::requestMaximize, this, &SurfaceWrapper::requestMaximize);
    shellSurface->safeConnect(&WToplevelSurface::requestCancelMaximize, this, &SurfaceWrapper::requestCancelMaximize);
    shellSurface->safeConnect(&WToplevelSurface::requestMove, this, [this](WSeat *, quint32) {
        Q_EMIT requestMove();
    });
    shellSurface->safeConnect(&WToplevelSurface::requestResize, this, [this](WSeat *, Qt::Edges edge, quint32) {
        Q_EMIT requestResize(edge);
    });
    shellSurface->safeConnect(&WToplevelSurface::requestFullscreen, this, &SurfaceWrapper::requestFullscreen);
    shellSurface->safeConnect(&WToplevelSurface::requestCancelFullscreen, this, &SurfaceWrapper::requestCancelFullscreen);
    if (type == Type::XdgToplevel) {
        shellSurface->safeConnect(&WToplevelSurface::requestShowWindowMenu, this, [this](WSeat *, QPoint pos, quint32) {
            Q_EMIT requestShowWindowMenu(pos);
        });
    }

    connect(m_surfaceItem, &WSurfaceItem::boundingRectChanged, this, &SurfaceWrapper::updateBoundingRect);
    connect(m_surfaceItem, &WSurfaceItem::implicitWidthChanged, this, [this] {
        setImplicitWidth(m_surfaceItem->implicitWidth());
    });
    connect(m_surfaceItem, &WSurfaceItem::implicitHeightChanged, this, [this] {
        setImplicitHeight(m_surfaceItem->implicitHeight());
    });
    setImplicitSize(m_surfaceItem->implicitWidth(), m_surfaceItem->implicitHeight());

    if (!shellSurface->hasCapability(WToplevelSurface::Capability::Focus)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        m_surfaceItem->setFocusPolicy(Qt::NoFocus);
#endif
    }

    if (type == Type::XWayland) {
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(shellSurface);
        auto xwaylandSurfaceItem = qobject_cast<WXWaylandSurfaceItem*>(m_surfaceItem);

        connect(xwaylandSurfaceItem, &WXWaylandSurfaceItem::implicitPositionChanged, this, [this, xwaylandSurfaceItem]() {
            if (m_xwaylandPositionFromSurface)
                moveNormalGeometryInOutput(xwaylandSurfaceItem->implicitPosition());
        });

        connect(this, &QQuickItem::xChanged, xwaylandSurface, [this, xwaylandSurfaceItem]() {
            xwaylandSurfaceItem->moveTo(position(), !m_xwaylandPositionFromSurface);
        });

        connect(this, &QQuickItem::yChanged, xwaylandSurface, [this, xwaylandSurfaceItem]() {
            xwaylandSurfaceItem->moveTo(position(), !m_xwaylandPositionFromSurface);
        });

        auto requestPos = xwaylandSurface->requestConfigureGeometry().topLeft();
        if (!requestPos.isNull()) {
            m_positionAutomatic = false;
            moveNormalGeometryInOutput(xwaylandSurfaceItem->implicitPosition());
        }
    }
}

SurfaceWrapper::~SurfaceWrapper()
{
    if (m_titleBar)
        delete m_titleBar;
    if (m_decoration)
        delete m_decoration;
    if (m_geometryAnimation)
        delete m_geometryAnimation;

    if (m_ownsOutput) {
        m_ownsOutput->removeSurface(this);
        m_ownsOutput = nullptr;
    }

    if (m_container) {
        m_container->removeSurface(this);
        m_container = nullptr;
    }

    for (auto subS : std::as_const(m_subSurfaces)) {
        subS->m_parentSurface = nullptr;
    }

    if (m_parentSurface)
        m_parentSurface->removeSubSurface(this);
}

void SurfaceWrapper::setParent(QQuickItem *item)
{
    QObject::setParent(item);
    setParentItem(item);
}

void SurfaceWrapper::setActivate(bool activate)
{
    m_shellSurface->setActivate(activate);
    auto parent = parentSurface();
    while (parent) {
        parent->setActivate(activate);
        parent = parent->parentSurface();
    }
}

void SurfaceWrapper::setFocus(bool focus, Qt::FocusReason reason)
{
    if (focus)
        m_surfaceItem->forceActiveFocus(reason);
    else
        m_surfaceItem->setFocus(false, reason);
}

WSurface *SurfaceWrapper::surface() const
{
    return m_shellSurface->surface();
}

WToplevelSurface *SurfaceWrapper::shellSurface() const
{
    return m_shellSurface;
}

WSurfaceItem *SurfaceWrapper::surfaceItem() const
{
    return m_surfaceItem;
}

bool SurfaceWrapper::resize(const QSizeF &size)
{
    return m_surfaceItem->resizeSurface(size);
}

QRectF SurfaceWrapper::titlebarGeometry() const
{
    return m_titleBar ? QRectF({0, 0}, m_titleBar->size()) : QRectF();
}

QRectF SurfaceWrapper::boundingRect() const
{
    return m_boundedRect;
}

QRectF SurfaceWrapper::normalGeometry() const
{
    return m_normalGeometry;
}

void SurfaceWrapper::moveNormalGeometryInOutput(const QPointF &position)
{
    setNormalGeometry(QRectF(position, m_normalGeometry.size()));
    if (isNormal()) {
        setPosition(position);
    } else if (m_pendingState == State::Normal && m_geometryAnimation) {
        m_geometryAnimation->setProperty("targetGeometry", m_normalGeometry);
    }
}

void SurfaceWrapper::setNormalGeometry(const QRectF &newNormalGeometry)
{
    if (m_normalGeometry == newNormalGeometry)
        return;
    m_normalGeometry = newNormalGeometry;
    emit normalGeometryChanged();
}

QRectF SurfaceWrapper::maximizedGeometry() const
{
    return m_maximizedGeometry;
}

void SurfaceWrapper::setMaximizedGeometry(const QRectF &newMaximizedGeometry)
{
    if (m_maximizedGeometry == newMaximizedGeometry)
        return;
    m_maximizedGeometry = newMaximizedGeometry;
    // This geometry change might be caused by a change in the output size due to screen scaling.
    // Ensure that the surfaceSizeRatio is updated before modifying the window size
    // to avoid incorrect sizing of Xwayland windows.
    updateSurfaceSizeRatio();

    if (m_surfaceState == State::Maximized) {
        setPosition(newMaximizedGeometry.topLeft());
        resize(newMaximizedGeometry.size());
    } else if (m_pendingState == State::Maximized && m_geometryAnimation) {
        m_geometryAnimation->setProperty("targetGeometry", newMaximizedGeometry);
    }

    emit maximizedGeometryChanged();
}

QRectF SurfaceWrapper::fullscreenGeometry() const
{
    return m_fullscreenGeometry;
}

void SurfaceWrapper::setFullscreenGeometry(const QRectF &newFullscreenGeometry)
{
    if (m_fullscreenGeometry == newFullscreenGeometry)
        return;
    m_fullscreenGeometry = newFullscreenGeometry;
    // This geometry change might be caused by a change in the output size due to screen scaling.
    // Ensure that the surfaceSizeRatio is updated before modifying the window size
    // to avoid incorrect sizing of Xwayland windows.
    updateSurfaceSizeRatio();

    if (m_surfaceState == State::Fullscreen) {
        setPosition(newFullscreenGeometry.topLeft());
        resize(newFullscreenGeometry.size());
    } else if (m_pendingState == State::Fullscreen && m_geometryAnimation) {
        m_geometryAnimation->setProperty("targetGeometry", newFullscreenGeometry);
    }

    emit fullscreenGeometryChanged();

    updateClipRect();
}

QRectF SurfaceWrapper::tilingGeometry() const
{
    return m_tilingGeometry;
}

void SurfaceWrapper::setTilingGeometry(const QRectF &newTilingGeometry)
{
    if (m_tilingGeometry == newTilingGeometry)
        return;
    m_tilingGeometry = newTilingGeometry;
    // This geometry change might be caused by a change in the output size due to screen scaling.
    // Ensure that the surfaceSizeRatio is updated before modifying the window size
    // to avoid incorrect sizing of Xwayland windows.
    updateSurfaceSizeRatio();

    if (m_surfaceState == State::Tiling) {
        setPosition(newTilingGeometry.topLeft());
        resize(newTilingGeometry.size());
    }

    emit tilingGeometryChanged();
}

bool SurfaceWrapper::positionAutomatic() const
{
    return m_positionAutomatic;
}

void SurfaceWrapper::setPositionAutomatic(bool newPositionAutomatic)
{
    if (m_positionAutomatic == newPositionAutomatic)
        return;
    m_positionAutomatic = newPositionAutomatic;
    emit positionAutomaticChanged();
}

void SurfaceWrapper::resetWidth()
{
    m_surfaceItem->resetWidth();
    QQuickItem::resetWidth();
}

void SurfaceWrapper::resetHeight()
{
    m_surfaceItem->resetHeight();
    QQuickItem::resetHeight();
}

SurfaceWrapper::Type SurfaceWrapper::type() const
{
    return m_type;
}

SurfaceWrapper *SurfaceWrapper::parentSurface() const
{
    return m_parentSurface;
}

Output *SurfaceWrapper::ownsOutput() const
{
    return m_ownsOutput;
}

void SurfaceWrapper::setOwnsOutput(Output *newOwnsOutput)
{
    if (m_ownsOutput == newOwnsOutput)
        return;

    if (m_ownsOutput) {
        m_ownsOutput->removeSurface(this);
    }

    m_ownsOutput = newOwnsOutput;

    if (m_ownsOutput) {
        m_ownsOutput->addSurface(this);
    }

    emit ownsOutputChanged();
}

void SurfaceWrapper::setOutputs(const QList<WOutput*> &outputs)
{
    auto oldOutputs = surface()->outputs();
    for (auto output : oldOutputs) {
        if (outputs.contains(output)) {
            continue;
        }
        surface()->leaveOutput(output);
    }

    for (auto output : outputs) {
        if (oldOutputs.contains(output))
            continue;
        surface()->enterOutput(output);
    }
}

QRectF SurfaceWrapper::geometry() const
{
    return QRectF(position(), size());
}

SurfaceWrapper::State SurfaceWrapper::previousSurfaceState() const
{
    return m_previousSurfaceState;
}

SurfaceWrapper::State SurfaceWrapper::surfaceState() const
{
    return m_surfaceState;
}

void SurfaceWrapper::setSurfaceState(State newSurfaceState)
{
    if (m_geometryAnimation)
        return;

    if (m_surfaceState == newSurfaceState)
        return;

    if (container()->filterSurfaceStateChange(this, newSurfaceState, m_surfaceState))
        return;

    QRectF targetGeometry;

    if (newSurfaceState == State::Maximized) {
        targetGeometry = m_maximizedGeometry;
    } else if (newSurfaceState == State::Fullscreen) {
        targetGeometry = m_fullscreenGeometry;
    } else if (newSurfaceState == State::Normal) {
        targetGeometry = m_normalGeometry;
    } else if (newSurfaceState == State::Tiling) {
        targetGeometry = m_tilingGeometry;
    }

    if (targetGeometry.isValid()) {
        startStateChangeAnimation(newSurfaceState, targetGeometry);
    } else {
        if (m_geometryAnimation) {
            m_geometryAnimation->deleteLater();
        }

        doSetSurfaceState(newSurfaceState);
    }
}

QBindable<SurfaceWrapper::State> SurfaceWrapper::bindableSurfaceState()
{
    return &m_surfaceState;
}

bool SurfaceWrapper::isNormal() const
{
    return m_surfaceState == State::Normal;
}

bool SurfaceWrapper::isMaximized() const
{
    return m_surfaceState == State::Maximized;
}

bool SurfaceWrapper::isMinimized() const
{
    return m_surfaceState == State::Minimized;
}

bool SurfaceWrapper::isTiling() const
{
    return m_surfaceState == State::Tiling;
}

bool SurfaceWrapper::isAnimationRunning() const
{
    return m_geometryAnimation;
}

void SurfaceWrapper::setNoDecoration(bool newNoDecoration)
{
    setNoCornerRadius(newNoDecoration);
    if (m_noDecoration == newNoDecoration)
        return;

    m_noDecoration = newNoDecoration;
    if (m_titleBarState == TitleBarState::Default)
        updateTitleBar();

    if (m_noDecoration) {
        Q_ASSERT(m_decoration);
        m_decoration->deleteLater();
        m_decoration = nullptr;
    } else {
        Q_ASSERT(!m_decoration);
        m_decoration = m_engine->createDecoration(this, this);
        m_decoration->stackBefore(m_surfaceItem);
        connect(m_decoration, &QQuickItem::xChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration, &QQuickItem::yChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration, &QQuickItem::widthChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration, &QQuickItem::heightChanged, this, &SurfaceWrapper::updateBoundingRect);
    }

    updateBoundingRect();
    emit noDecorationChanged();
}

void SurfaceWrapper::updateTitleBar()
{
    if (noTitleBar() == !m_titleBar)
        return;

    if (m_titleBar) {
        m_titleBar->deleteLater();
        m_titleBar = nullptr;
        m_surfaceItem->setTopPadding(0);
    } else {
        m_titleBar = m_engine->createTitleBar(this, m_surfaceItem);
        m_titleBar->setZ(static_cast<int>(WSurfaceItem::ZOrder::ContentItem));
        if (type() == Type::XWayland) {
            auto xwaylandSurfaceItem = qobject_cast<WXWaylandSurfaceItem*>(m_surfaceItem);
            xwaylandSurfaceItem->moveTo(QPointF(0, m_titleBar->height()), !m_xwaylandPositionFromSurface);
        }
        m_surfaceItem->setTopPadding(m_titleBar->height());
        connect(m_titleBar, &QQuickItem::heightChanged, this, [this] {
            m_surfaceItem->setTopPadding(m_titleBar->height());
        });
    }

    emit noTitleBarChanged();
}

void SurfaceWrapper::setBoundedRect(const QRectF &newBoundedRect)
{
    if (m_boundedRect == newBoundedRect)
        return;
    m_boundedRect = newBoundedRect;
    emit boundingRectChanged();
}

void SurfaceWrapper::updateBoundingRect()
{
    QRectF rect(QRectF(QPointF(0, 0), size()));
    rect |= m_surfaceItem->boundingRect();

    if (!m_decoration || !m_visibleDecoration) {
        setBoundedRect(rect);
        return;
    }

    const QRectF dr(m_decoration->position(), m_decoration->size());
    setBoundedRect(dr | rect);
}

void SurfaceWrapper::updateVisible()
{
    setVisible(!isMinimized() && surface()->mapped());
}

void SurfaceWrapper::updateSubSurfaceStacking()
{
    SurfaceWrapper *lastSurface = this;
    for (auto surface : std::as_const(m_subSurfaces)) {
        surface->stackAfter(lastSurface);
        lastSurface = surface->stackLastSurface();
    }
}

void SurfaceWrapper::updateClipRect()
{
    if (!clip() || !window())
        return;
    auto rw = qobject_cast<WOutputRenderWindow*>(window());
    Q_ASSERT(rw);
    rw->markItemClipRectDirty(this);
}

void SurfaceWrapper::geometryChange(const QRectF &newGeo, const QRectF &oldGeometry)
{
    QRectF newGeometry = newGeo;
    if (m_container && m_container->filterSurfaceGeometryChanged(this, newGeometry, oldGeometry))
        return;

    if (isNormal() && !m_geometryAnimation) {
        setNormalGeometry(newGeometry);
    }

    if (widthValid() && heightValid()) {
        resize(newGeometry.size());
    }

    Q_EMIT geometryChanged();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        updateBoundingRect();
    updateClipRect();
}

void SurfaceWrapper::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemSceneChange) {
        updateSurfaceSizeRatio();
    }

    return QQuickItem::itemChange(change, data);
}

void SurfaceWrapper::doSetSurfaceState(State newSurfaceState)
{
    setVisibleDecoration(newSurfaceState == State::Normal);
    setNoCornerRadius(newSurfaceState != State::Normal);

    m_previousSurfaceState.setValueBypassingBindings(m_surfaceState);
    m_surfaceState.setValueBypassingBindings(newSurfaceState);

    switch (m_previousSurfaceState.value()) {
    case State::Maximized:
        m_shellSurface->setMaximize(false);
        break;
    case State::Minimized:
        m_shellSurface->setMinimize(false);
        break;
    case State::Fullscreen:
        m_shellSurface->setFullScreen(false);
        break;
    case State::Normal: [[fallthrough]];
    case State::Tiling: [[fallthrough]];
    default:
        break;
    }
    m_previousSurfaceState.notify();

    switch (m_surfaceState.value()) {
    case State::Maximized:
        m_shellSurface->setMaximize(true);
        break;
    case State::Minimized:
        m_shellSurface->setMinimize(true);
        break;
    case State::Fullscreen:
        m_shellSurface->setFullScreen(true);
        break;
    case State::Normal: [[fallthrough]];
    case State::Tiling: [[fallthrough]];
    default:
        break;
    }
    m_surfaceState.notify();
    updateTitleBar();
    updateVisible();
}

void SurfaceWrapper::onAnimationReady()
{
    Q_ASSERT(m_pendingState != m_surfaceState);
    Q_ASSERT(m_pendingGeometry.isValid());

    if (!resize(m_pendingGeometry.size())) {
        // abort change state if resize failed
        m_geometryAnimation->deleteLater();
        return;
    }

    setPosition(m_pendingGeometry.topLeft());
    doSetSurfaceState(m_pendingState);
}

void SurfaceWrapper::onAnimationFinished()
{
    setXwaylandPositionFromSurface(true);

    Q_ASSERT(m_geometryAnimation);
    m_geometryAnimation->deleteLater();
}

bool SurfaceWrapper::startStateChangeAnimation(State targetState, const QRectF &targetGeometry)
{
    if (m_geometryAnimation) // animation running
        return false;

    m_geometryAnimation = m_engine->createGeometryAnimation(this, geometry(), targetGeometry, container());
    m_pendingState = targetState;
    m_pendingGeometry = targetGeometry;
    bool ok = connect(m_geometryAnimation, SIGNAL(ready()), this, SLOT(onAnimationReady()));
    Q_ASSERT(ok);
    ok = connect(m_geometryAnimation, SIGNAL(finished()), this, SLOT(onAnimationFinished()));
    Q_ASSERT(ok);

    setXwaylandPositionFromSurface(false);
    ok = QMetaObject::invokeMethod(m_geometryAnimation, "start");
    Q_ASSERT(ok);
    return ok;
}

qreal SurfaceWrapper::radius() const
{
    // RoundedClipEffect is use ShaderEffectSource to clip, its only
    // supports RHI backend.
    if (window()->sceneGraphBackend() == "software")
        return 0;

    return m_radius;
}

void SurfaceWrapper::setRadius(qreal newRadius)
{
    if (qFuzzyCompare(m_radius, newRadius))
        return;
    m_radius = newRadius;
    emit radiusChanged();
}

void SurfaceWrapper::requestMinimize()
{
    setSurfaceState(State::Minimized);
}

void SurfaceWrapper::requestCancelMinimize()
{
    if (m_surfaceState != State::Minimized)
        return;

    setSurfaceState(m_previousSurfaceState);
}

void SurfaceWrapper::requestMaximize()
{
    if (m_surfaceState == State::Minimized || m_surfaceState == State::Fullscreen)
        return;

    setSurfaceState(State::Maximized);
}

void SurfaceWrapper::requestCancelMaximize()
{
    if (m_surfaceState != State::Maximized)
        return;

    setSurfaceState(State::Normal);
}

void SurfaceWrapper::requestToggleMaximize()
{
    if (m_surfaceState == State::Maximized)
        requestCancelMaximize();
    else
        requestMaximize();
}

void SurfaceWrapper::requestFullscreen()
{
    if (m_surfaceState == State::Minimized)
        return;

    setSurfaceState(State::Fullscreen);
}

void SurfaceWrapper::requestCancelFullscreen()
{
    if (m_surfaceState != State::Fullscreen)
        return;

    setSurfaceState(m_previousSurfaceState);
}

void SurfaceWrapper::requestClose()
{
    m_shellSurface->close();
    updateVisible();
}

SurfaceWrapper *SurfaceWrapper::stackFirstSurface() const
{
    return m_subSurfaces.isEmpty() ? const_cast<SurfaceWrapper*>(this) : m_subSurfaces.first()->stackFirstSurface();
}

SurfaceWrapper *SurfaceWrapper::stackLastSurface() const
{
    return m_subSurfaces.isEmpty() ? const_cast<SurfaceWrapper*>(this) : m_subSurfaces.last()->stackLastSurface();
}

bool SurfaceWrapper::hasChild(SurfaceWrapper *child) const
{
    for (auto s : std::as_const(m_subSurfaces)) {
        if (s == child || s->hasChild(child))
            return true;
    }

    return false;
}

bool SurfaceWrapper::stackBefore(QQuickItem *item)
{
    if (!parentItem() || item->parentItem() != parentItem())
        return false;
    if (this == item)
        return false;

    do {
        auto s = qobject_cast<SurfaceWrapper*>(item);
        if (s) {
            if (s->hasChild(this))
                return false;
            if (hasChild(s)) {
                QQuickItem::stackBefore(item);
                break;
            }
            item = s->stackFirstSurface();

            if (m_parentSurface && m_parentSurface == s->m_parentSurface) {
                QQuickItem::stackBefore(item);

                int myIndex = m_parentSurface->m_subSurfaces.lastIndexOf(this);
                int siblingIndex = m_parentSurface->m_subSurfaces.lastIndexOf(s);
                Q_ASSERT(myIndex != -1 && siblingIndex != -1);
                if (myIndex != siblingIndex - 1)
                    m_parentSurface->m_subSurfaces.move(myIndex,
                                                        myIndex < siblingIndex ? siblingIndex - 1 : siblingIndex);
                break;
            }
        }

        if (m_parentSurface) {
            if (!m_parentSurface->stackBefore(item))
                return false;
        } else {
            QQuickItem::stackBefore(item);
        }
    } while (false);

    updateSubSurfaceStacking();
    return true;
}

bool SurfaceWrapper::stackAfter(QQuickItem *item)
{
    if (!parentItem() || item->parentItem() != parentItem())
        return false;
    if (this == item)
        return false;

    do {
        auto s = qobject_cast<SurfaceWrapper*>(item);
        if (s) {
            if (hasChild(s))
                return false;
            if (s->hasChild(this)) {
                QQuickItem::stackAfter(item);
                break;
            }
            item = s->stackLastSurface();

            if (m_parentSurface && m_parentSurface == s->m_parentSurface) {
                QQuickItem::stackAfter(item);

                int myIndex = m_parentSurface->m_subSurfaces.lastIndexOf(this);
                int siblingIndex = m_parentSurface->m_subSurfaces.lastIndexOf(s);
                Q_ASSERT(myIndex != -1 && siblingIndex != -1);
                if (myIndex != siblingIndex + 1)
                    m_parentSurface->m_subSurfaces.move(myIndex,
                                                        myIndex > siblingIndex ? siblingIndex + 1 : siblingIndex);

                break;
            }
        }

        if (m_parentSurface) {
            if (!m_parentSurface->stackAfter(item))
                return false;
        } else {
            QQuickItem::stackAfter(item);
        }
    } while (false);

    updateSubSurfaceStacking();
    return true;
}

void SurfaceWrapper::stackToLast()
{
    if (!parentItem())
        return;

    if (m_parentSurface) {
        m_parentSurface->stackToLast();
        stackAfter(m_parentSurface->stackLastSurface());
    } else {
        auto last = parentItem()->childItems().last();
        stackAfter(last);
    }
}

void SurfaceWrapper::addSubSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(!surface->m_parentSurface);
    surface->m_parentSurface = this;
    surface->updateExplicitAlwaysOnTop();
    m_subSurfaces.append(surface);
}

void SurfaceWrapper::removeSubSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(surface->m_parentSurface == this);
    surface->m_parentSurface = nullptr;
    surface->updateExplicitAlwaysOnTop();
    m_subSurfaces.removeOne(surface);
}

const QList<SurfaceWrapper *> &SurfaceWrapper::subSurfaces() const
{
    return m_subSurfaces;
}

SurfaceContainer *SurfaceWrapper::container() const
{
    return m_container;
}

void SurfaceWrapper::setContainer(SurfaceContainer *newContainer)
{
    if (m_container == newContainer)
        return;
    m_container = newContainer;
    emit containerChanged();
}

QQuickItem *SurfaceWrapper::titleBar() const
{
    return m_titleBar;
}

QQuickItem *SurfaceWrapper::decoration() const
{
    return m_decoration;
}

bool SurfaceWrapper::noDecoration() const
{
    return m_noDecoration;
}

bool SurfaceWrapper::visibleDecoration() const
{
    return m_visibleDecoration;
}

void SurfaceWrapper::setVisibleDecoration(bool newVisibleDecoration)
{
    if (m_visibleDecoration == newVisibleDecoration)
        return;
    m_visibleDecoration = newVisibleDecoration;
    updateBoundingRect();
    emit visibleDecorationChanged();
}

bool SurfaceWrapper::clipInOutput() const
{
    return m_clipInOutput;
}

void SurfaceWrapper::setClipInOutput(bool newClipInOutput)
{
    if (m_clipInOutput == newClipInOutput)
        return;
    m_clipInOutput = newClipInOutput;
    updateClipRect();
    emit clipInOutputChanged();
}

QRectF SurfaceWrapper::clipRect() const
{
    if (m_clipInOutput) {
        return m_fullscreenGeometry & geometry();
    }

    return QQuickItem::clipRect();
}

bool SurfaceWrapper::noTitleBar() const
{
    if (m_surfaceState == State::Fullscreen)
        return true;
    if (m_titleBarState == TitleBarState::Visible)
        return false;

    return m_titleBarState == TitleBarState::Hidden || m_noDecoration;
}

void SurfaceWrapper::setNoTitleBar(bool newNoTitleBar)
{
    if (newNoTitleBar) {
        m_titleBarState = TitleBarState::Hidden;
    } else {
        m_titleBarState = TitleBarState::Visible;
    }
    updateTitleBar();
}

void SurfaceWrapper::resetNoTitleBar()
{
    m_titleBarState = TitleBarState::Default;
    updateTitleBar();
}

bool SurfaceWrapper::noCornerRadius() const
{
    return m_noCornerRadius;
}

void SurfaceWrapper::setNoCornerRadius(bool newNoCornerRadius)
{
    if (m_noCornerRadius == newNoCornerRadius)
        return;
    m_noCornerRadius = newNoCornerRadius;
    emit noCornerRadiusChanged();
}

int SurfaceWrapper::workspaceId() const
{
    return m_workspaceId;
}

void SurfaceWrapper::setWorkspaceId(int newWorkspaceId)
{
    if (m_workspaceId == newWorkspaceId)
        return;

    bool onAllWorkspaceHasChanged = m_workspaceId == 0 || newWorkspaceId == 0;
    m_workspaceId = newWorkspaceId;

    if (onAllWorkspaceHasChanged)
        Q_EMIT showOnAllWorkspaceChanged();
    Q_EMIT workspaceIdChanged();
}

bool SurfaceWrapper::alwaysOnTop() const
{
    return m_alwaysOnTop;
}

void SurfaceWrapper::setAlwaysOnTop(bool alwaysOnTop)
{
    if (m_alwaysOnTop == alwaysOnTop)
        return;
    m_alwaysOnTop = alwaysOnTop;
    updateExplicitAlwaysOnTop();

    Q_EMIT alwaysOnTopChanged();
}

bool SurfaceWrapper::showOnAllWorkspace() const
{
    return m_workspaceId == 0;
}

void SurfaceWrapper::updateExplicitAlwaysOnTop()
{
    int newExplicitAlwaysOnTop = m_alwaysOnTop;
    if (m_parentSurface)
        newExplicitAlwaysOnTop += m_parentSurface->m_explicitAlwaysOnTop;

    if (m_explicitAlwaysOnTop == newExplicitAlwaysOnTop)
        return;

    m_explicitAlwaysOnTop = newExplicitAlwaysOnTop;
    setZ(m_explicitAlwaysOnTop ? 1 : 0);
    for (const auto& sub : std::as_const(m_subSurfaces))
        sub->updateExplicitAlwaysOnTop();
}

void SurfaceWrapper::updateSurfaceSizeRatio()
{
    if (m_type == Type::XWayland && m_surfaceItem && window()) {
        const qreal targetScale = window()->effectiveDevicePixelRatio();
        if (m_surfaceItem->bufferScale() < targetScale)
            m_surfaceItem->setSurfaceSizeRatio(targetScale / m_surfaceItem->bufferScale());
        else
            m_surfaceItem->setSurfaceSizeRatio(1.0);
    }
}

void SurfaceWrapper::setXwaylandPositionFromSurface(bool value)
{
    m_xwaylandPositionFromSurface = value;
}
