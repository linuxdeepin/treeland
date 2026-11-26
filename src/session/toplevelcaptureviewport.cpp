// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "toplevelcaptureviewport.h"

#include "core/qmlengine.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"

#include <wsurfaceitem.h>
#include <woutputrenderwindow.h>
#include <QQuickRenderControl>
#include <private/qquickitem_p.h>
#include <private/qquickshadereffectsource_p.h>
#include <QtMath>

namespace {
class CaptureSurfaceObserver final : public QObject
{
public:
    CaptureSurfaceObserver(WSurfaceItem *item, WOutputViewport *viewport)
        : QObject(item), m_viewport(viewport)
    {
        connect(item->surface(), &WSurface::commit, this, &CaptureSurfaceObserver::committed);
        connect(item->surface(), &WSurface::subsurfaceOrderChanged, this, &CaptureSurfaceObserver::changed);
        connect(item, &WSurfaceItem::subsurfaceAdded, this, &CaptureSurfaceObserver::observeSubsurface);
        connect(item, &WSurfaceItem::subsurfaceRemoved, this, &CaptureSurfaceObserver::changed);
        connect(item, &WSurfaceItem::xChanged, this, &CaptureSurfaceObserver::changed);
        connect(item, &WSurfaceItem::yChanged, this, &CaptureSurfaceObserver::changed);
        connect(item, &WSurfaceItem::visibleChanged, this, &CaptureSurfaceObserver::changed);
        const auto children = item->findChildren<WSurfaceItem *>(QString(), Qt::FindDirectChildrenOnly);
        for (auto *child : children)
            new CaptureSurfaceObserver(child, viewport);
    }

private:
    void observeSubsurface(WSurfaceItem *item)
    {
        new CaptureSurfaceObserver(item, m_viewport);
        changed();
    }

    void committed(quint32 state)
    {
        constexpr quint32 visualState = WLR_SURFACE_STATE_BUFFER | WLR_SURFACE_STATE_SURFACE_DAMAGE
            | WLR_SURFACE_STATE_BUFFER_DAMAGE | WLR_SURFACE_STATE_TRANSFORM | WLR_SURFACE_STATE_SCALE
            | WLR_SURFACE_STATE_VIEWPORT | WLR_SURFACE_STATE_OFFSET;
        if (state & visualState)
            changed();
    }

    void changed() { Q_EMIT m_viewport->contentChanged(); }
    WOutputViewport *const m_viewport;
};

// A second view of the committed wl_surface tree, not a SurfaceWrapper proxy:
// those can configure/activate the shared shell surface. Capture is read-only.
class CaptureSurfaceItem final : public WSurfaceItem
{
public:
    CaptureSurfaceItem(SurfaceWrapper *wrapper, QQuickItem *parent)
        : WSurfaceItem(parent)
        , m_wrapper(wrapper)
    {
        setResizeMode(ManualResize);
        setFlags(RejectEvent);
    }

protected:
    bool doResizeSurface(const QSize &) override { return false; }

    QRectF getContentGeometry() const override
    {
        return shellSurface()->getContentGeometry();
    }

    QSizeF getContentSize() const override
    {
        return m_wrapper->type() == SurfaceWrapper::Type::XWayland
            ? getContentGeometry().size() : WSurfaceItem::getContentSize();
    }

private:
    SurfaceWrapper *const m_wrapper;
};
}

ToplevelCaptureViewport::ToplevelCaptureViewport(SurfaceWrapper *wrapper, WOutput *output)
    : WOutputViewport(wrapper->surfaceItem()->window()->contentItem())
    , m_wrapper(wrapper)
    , m_surfaceItem(new CaptureSurfaceItem(wrapper, this))
{
    setVisible(false);
    setOffscreen(true);
    setHideSource(false);
    setIgnoreViewport(true);
    setColorContentsMode(WGlobal::ColorContentsMode::Clear);

    auto *engine = WaylandServer::instance()->qmlEngine();
    QQmlEngine::setContextForObject(this, engine->rootContext());
    QQmlEngine::setContextForObject(m_surfaceItem, engine->rootContext());
    m_surfaceItem->setDelegate(engine->captureSurfaceContentComponent());
    m_surfaceItem->setShellSurface(wrapper->shellSurface());

    // Cull the private subtree from the main scene and input, not its effect renderer.
    QQuickItemPrivate::get(m_surfaceItem)->setCulled(true);
    setInput(m_surfaceItem);
    setOutput(output);
    new CaptureSurfaceObserver(m_surfaceItem, this);
    // Check after the current update batch, before a disabled output needs a frame.
    connect(outputRenderWindow()->renderControl(), &QQuickRenderControl::sceneChanged,
            this, &ToplevelCaptureViewport::checkContentChanges, Qt::QueuedConnection);

    auto *source = wrapper->surfaceItem();
    connect(source, &WSurfaceItem::widthChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::heightChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::surfaceSizeRatioChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::topPaddingChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::bottomPaddingChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::leftPaddingChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::rightPaddingChanged,
            this, &ToplevelCaptureViewport::updateSurfaceGeometry);
    connect(source, &WSurfaceItem::flagsChanged,
            this, &ToplevelCaptureViewport::updateSurfaceFlags);
    connect(source, &WSurfaceItem::smoothChanged, m_surfaceItem, &WSurfaceItem::setSmooth);
    connect(m_surfaceItem, &WSurfaceItem::boundingRectChanged,
            this, &ToplevelCaptureViewport::updateGeometry);
    connect(wrapper, &SurfaceWrapper::noTitleBarChanged,
            this, &ToplevelCaptureViewport::updateDecoration);
    connect(wrapper, &SurfaceWrapper::noDecorationChanged,
            this, &ToplevelCaptureViewport::updateDecoration);
    connect(wrapper, &SurfaceWrapper::visibleDecorationChanged,
            this, &ToplevelCaptureViewport::updateDecoration);
    connect(wrapper, &SurfaceWrapper::borderChanged,
            this, &ToplevelCaptureViewport::updateDecoration);
    connect(wrapper, &SurfaceWrapper::radiusChanged,
            this, &ToplevelCaptureViewport::updateDecoration);
    connect(output, &WOutput::scaleChanged,
            this, &ToplevelCaptureViewport::updateGeometry);

    updateSurfaceFlags();
    m_surfaceItem->setSmooth(source->smooth());
    updateSurfaceGeometry();
}

SurfaceWrapper *ToplevelCaptureViewport::sourceSurface() const
{
    return m_wrapper;
}

ToplevelCaptureViewport::~ToplevelCaptureViewport()
{
    for (auto *source : std::as_const(m_textureSources))
        source->disconnect(this);
}

bool ToplevelCaptureViewport::hasDirtyContent(QQuickItem *item)
{
    auto *d = QQuickItemPrivate::get(item);
    quint32 dirty = d->dirtyAttributes;
    if (auto *source = qobject_cast<QQuickShaderEffectSource *>(item)) {
        watchTextureSource(source);
        // A layer texture refresh marks Content even when its source is unchanged.
        if (!source->recursive())
            dirty &= ~QQuickItemPrivate::Content;
    }
    bool changed = dirty != 0;
    for (auto *child : std::as_const(d->childItems))
        changed |= hasDirtyContent(child);
    return changed;
}

void ToplevelCaptureViewport::watchTextureSource(QQuickItem *item)
{
    auto *source = static_cast<QQuickShaderEffectSource *>(item);
    if (m_textureSources.contains(source))
        return;
    m_textureSources.insert(source);
    connect(source, &QObject::destroyed, this, [this](QObject *object) { m_textureSources.remove(object); });
    connect(source, &QQuickShaderEffectSource::sourceItemChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::sourceRectChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::textureSizeChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::formatChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::wrapModeChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::mipmapChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::textureMirroringChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::samplesChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::recursiveChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::hideSourceChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::liveChanged, this, &WOutputViewport::contentChanged);
    connect(source, &QQuickShaderEffectSource::scheduledUpdateCompleted,
            this, &ToplevelCaptureViewport::textureUpdated);
}

void ToplevelCaptureViewport::textureUpdated()
{
    auto *source = qobject_cast<QQuickShaderEffectSource *>(sender());
    if (source && !source->live())
        Q_EMIT contentChanged();
}

void ToplevelCaptureViewport::checkContentChanges()
{
    if (hasDirtyContent(m_surfaceItem))
        Q_EMIT contentChanged();
}

void ToplevelCaptureViewport::updateSurfaceFlags()
{
    m_surfaceItem->setFlags(m_wrapper->surfaceItem()->flags() | WSurfaceItem::RejectEvent);
}

void ToplevelCaptureViewport::updateSurfaceGeometry()
{
    auto *source = m_wrapper->surfaceItem();
    m_surfaceItem->setSurfaceSizeRatio(source->surfaceSizeRatio());
    m_surfaceItem->setTopPadding(source->topPadding());
    m_surfaceItem->setBottomPadding(source->bottomPadding());
    m_surfaceItem->setLeftPadding(source->leftPadding());
    m_surfaceItem->setRightPadding(source->rightPadding());
    m_surfaceItem->setSize(source->size());
    updateDecoration();
}

void ToplevelCaptureViewport::updateDecoration()
{
    auto *engine = WaylandServer::instance()->qmlEngine();
    if (m_wrapper->noTitleBar()) {
        if (m_titleBar) {
            m_titleBar->setVisible(false);
            m_titleBar->deleteLater();
            m_titleBar = nullptr;
        }
    } else if (!m_titleBar) {
        m_titleBar = engine->createTitleBar(m_wrapper, m_surfaceItem);
        m_titleBar->setZ(static_cast<int>(WSurfaceItem::ZOrder::ContentItem));
    }

    const bool borderVisible = !m_wrapper->noDecoration() && m_wrapper->visibleDecoration()
        && m_wrapper->borderVisible();
    if (borderVisible && !m_border) {
        m_border = engine->createBorder(m_surfaceItem);
        m_border->setZ(static_cast<int>(WSurfaceItem::ZOrder::ContentItem) + 1);
    }
    if (m_border) {
        m_border->setVisible(borderVisible);
        m_border->setSize(m_surfaceItem->size());
        m_border->setProperty("radius", m_wrapper->radius());
        m_border->setProperty("outsideColor", m_wrapper->borderColor());
        m_border->setProperty("borderWidth", m_wrapper->borderWidth());
    }
    updateGeometry();
}

void ToplevelCaptureViewport::updateGeometry()
{
    if (!output())
        return;
    const qreal dpr = output()->scale();
    QRectF bounds = m_surfaceItem->boundingRect();
    if (!m_wrapper->noDecoration() && m_wrapper->visibleDecoration()
        && m_wrapper->borderVisible()) {
        // Only the outer SSD stroke extends beyond the surface tree. A shadow
        // is never created, and all bounds are local to the private input.
        const qreal border = m_wrapper->borderWidth();
        bounds |= QRectF(QPointF(), m_surfaceItem->size()).adjusted(-border, -border, border, border);
    }

    const QRect pixels(QPoint(qFloor(bounds.left() * dpr), qFloor(bounds.top() * dpr)),
                       QPoint(qCeil(bounds.right() * dpr) - 1, qCeil(bounds.bottom() * dpr) - 1));
    setDevicePixelRatio(dpr);
    setSize(QSizeF(pixels.size()) / dpr);
    setSourceRect(QRectF(QPointF(pixels.topLeft()) / dpr, size()));
    setBufferSize(pixels.size());
}
