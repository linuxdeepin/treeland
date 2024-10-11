// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output.h"

#include "helper.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"

#include <winputpopupsurface.h>
#include <wlayersurface.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>
#include <wxdgsurface.h>
#include <woutputlayout.h>
#include <wquicktextureproxy.h>

#include <QQmlEngine>

Q_LOGGING_CATEGORY(qLcLayerShell, "Treeland.shell.layer", QtWarningMsg)

Output *Output::createPrimary(WOutput *output, QQmlEngine *engine, QObject *parent)
{
    QQmlComponent delegate(engine, "Treeland", "PrimaryOutput");
    QObject *obj = delegate.beginCreate(engine->rootContext());
    delegate.setInitialProperties(obj, {
                                           {"forceSoftwareCursor", output->handle()->is_x11()}
                                       });
    delegate.completeCreate();
    WOutputItem *outputItem = qobject_cast<WOutputItem *>(obj);
    Q_ASSERT(outputItem);
    QQmlEngine::setObjectOwnership(outputItem, QQmlEngine::CppOwnership);
    outputItem->setOutput(output);

    auto o = new Output(outputItem, parent);
    o->m_type = Type::Primary;
    obj->setParent(o);

    o->minimizedSurfaces->setFilter([](SurfaceWrapper *s) {
        return s->isMinimized();
    });

    o->connect(outputItem, &WOutputItem::geometryChanged, o, &Output::layoutAllSurfaces);

    auto contentItem = Helper::instance()->window()->contentItem();
    outputItem->setParentItem(contentItem);
    o->m_taskBar = Helper::instance()->qmlEngine()->createTaskBar(o, contentItem);
    o->m_taskBar->setZ(RootSurfaceContainer::TaskBarZOrder);

    o->m_menuBar = Helper::instance()->qmlEngine()->createMenuBar(outputItem, contentItem);
    o->m_menuBar->setZ(RootSurfaceContainer::MenuBarZOrder);
    o->setExclusiveZone(Qt::TopEdge, o->m_menuBar, o->m_menuBar->height());

    return o;
}

Output *Output::createCopy(WOutput *output, Output *proxy, QQmlEngine *engine, QObject *parent)
{
    QQmlComponent delegate(engine, "Treeland", "CopyOutput");
    QObject *obj = delegate.createWithInitialProperties({
                                                            {"targetOutputItem", QVariant::fromValue(proxy->outputItem())},
                                                        }, engine->rootContext());
    WOutputItem *outputItem = qobject_cast<WOutputItem *>(obj);
    Q_ASSERT(outputItem);
    QQmlEngine::setObjectOwnership(outputItem, QQmlEngine::CppOwnership);
    outputItem->setOutput(output);

    auto o = new Output(outputItem, parent);
    o->m_type = Type::Proxy;
    o->m_proxy = proxy;
    obj->setParent(o);

    auto contentItem = Helper::instance()->window()->contentItem();
    outputItem->setParentItem(contentItem);
    o->updatePrimaryOutputHardwareLayers();
    connect(o->m_outputViewport, &WOutputViewport::hardwareLayersChanged,
            o, &Output::updatePrimaryOutputHardwareLayers);

    return o;
}

Output::Output(WOutputItem *output, QObject *parent)
    : SurfaceListModel(parent)
    , m_item(output)
    , minimizedSurfaces(new SurfaceFilterModel(this))
{
    m_outputViewport = output->property("screenViewport").value<WOutputViewport *>();
}

Output::~Output()
{
    if (m_taskBar) {
        delete m_taskBar;
        m_taskBar = nullptr;
    }

    if (m_menuBar) {
        delete m_menuBar;
        m_menuBar = nullptr;
    }

    if (m_item) {
        delete m_item;
        m_item = nullptr;
    }
}

bool Output::isPrimary() const
{
    return m_type == Type::Primary;
}

void Output::updatePositionFromLayout()
{
    WOutputLayout * layout = output()->layout();
    Q_ASSERT(layout);
    auto *layoutOutput = layout->get(output()->nativeHandle());
    QPointF pos(layoutOutput->x, layoutOutput->y);
    m_item->setPosition(pos);
}
std::pair<WOutputViewport*, QQuickItem*> Output::getOutputItemProperty()
{
    WOutputViewport *viewportCopy = outputItem()->findChild<WOutputViewport*>({}, Qt::FindDirectChildrenOnly);
    Q_ASSERT(viewportCopy);
    auto textureProxy = outputItem()->findChild<WQuickTextureProxy*>();
    Q_ASSERT(textureProxy);
    return std::make_pair(viewportCopy, textureProxy);
}
void Output::updatePrimaryOutputHardwareLayers()
{
    WOutputViewport *viewportPrimary = screenViewport();
    std::pair<WOutputViewport*, QQuickItem*> copyOutput = getOutputItemProperty();
    const auto layers = viewportPrimary->hardwareLayers();
    for (auto layer : layers) {
        if (m_hardwareLayersOfPrimaryOutput.removeOne(layer))
            continue;
        Helper::instance()->window()->attach(layer, copyOutput.first, viewportPrimary, copyOutput.second);
    }
    for (auto oldLayer : std::as_const(m_hardwareLayersOfPrimaryOutput)) {
        Helper::instance()->window()->detach(oldLayer, copyOutput.first);
    }
    m_hardwareLayersOfPrimaryOutput = layers;
}

void Output::addSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(!hasSurface(surface));
    SurfaceListModel::addSurface(surface);

    if (surface->type() == SurfaceWrapper::Type::Layer) {
        auto layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
        layer->safeConnect(&WLayerSurface::layerPropertiesChanged,
                           this,
                           &Output::layoutLayerSurfaces);

        layoutLayerSurfaces();
    } else {
        auto layoutSurface = [surface, this] {
            layoutNonLayerSurface(surface, {});
        };

        connect(surface, &SurfaceWrapper::widthChanged, this, layoutSurface);
        connect(surface, &SurfaceWrapper::heightChanged, this, layoutSurface);
        layoutSurface();
    }
}

void Output::removeSurface(SurfaceWrapper *surface)
{
    Q_ASSERT(hasSurface(surface));
    SurfaceListModel::removeSurface(surface);
    surface->disconnect(this);

    if (surface->type() == SurfaceWrapper::Type::Layer) {
        if (auto ss = surface->shellSurface()) {
            ss->safeDisconnect(this);
            removeExclusiveZone(ss);
        }
        layoutLayerSurfaces();
    }
}

WOutput *Output::output() const
{
    auto o = m_item->output();
    Q_ASSERT(o);
    return o;
}

WOutputItem *Output::outputItem() const
{
    return m_item;
}

void Output::setExclusiveZone(Qt::Edge edge, QObject *object, int value)
{
    Q_ASSERT(value > 0);
    removeExclusiveZone(object);
    switch (edge) {
    case Qt::TopEdge:
        m_topExclusiveZones.append(std::make_pair(object, value));
        m_exclusiveZone.setTop(m_exclusiveZone.top() + value);
        break;
    case Qt::BottomEdge:
        m_bottomExclusiveZones.append(std::make_pair(object, value));
        m_exclusiveZone.setBottom(m_exclusiveZone.bottom() + value);
        break;
    case Qt::LeftEdge:
        m_leftExclusiveZones.append(std::make_pair(object, value));
        m_exclusiveZone.setLeft(m_exclusiveZone.left() + value);
        break;
    case Qt::RightEdge:
        m_rightExclusiveZones.append(std::make_pair(object, value));
        m_exclusiveZone.setRight(m_exclusiveZone.right() + value);
        break;
    default:
        Q_UNREACHABLE_RETURN();
    }
}

bool Output::removeExclusiveZone(QObject *object)
{
    auto finder = [object](const auto &pair) {
        return pair.first == object;
    };
    auto tmp = std::find_if(m_topExclusiveZones.begin(), m_topExclusiveZones.end(), finder);
    if (tmp != m_topExclusiveZones.end()) {
        m_topExclusiveZones.erase(tmp);
        m_exclusiveZone.setTop(m_exclusiveZone.top() - tmp->second);
        Q_ASSERT(m_exclusiveZone.top() >= 0);
        return true;
    }

    tmp = std::find_if(m_bottomExclusiveZones.begin(), m_bottomExclusiveZones.end(), finder);
    if (tmp != m_bottomExclusiveZones.end()) {
        m_bottomExclusiveZones.erase(tmp);
        m_exclusiveZone.setBottom(m_exclusiveZone.bottom() - tmp->second);
        Q_ASSERT(m_exclusiveZone.bottom() >= 0);
        return true;
    }

    tmp = std::find_if(m_leftExclusiveZones.begin(), m_leftExclusiveZones.end(), finder);
    if (tmp != m_leftExclusiveZones.end()) {
        m_leftExclusiveZones.erase(tmp);
        m_exclusiveZone.setLeft(m_exclusiveZone.left() - tmp->second);
        Q_ASSERT(m_exclusiveZone.left() >= 0);
        return true;
    }

    tmp = std::find_if(m_rightExclusiveZones.begin(), m_rightExclusiveZones.end(), finder);
    if (tmp != m_rightExclusiveZones.end()) {
        m_rightExclusiveZones.erase(tmp);
        m_exclusiveZone.setRight(m_exclusiveZone.right() - tmp->second);
        Q_ASSERT(m_exclusiveZone.right() >= 0);
        return true;
    }

    return false;
}

void Output::layoutLayerSurface(SurfaceWrapper *surface)
{
    WLayerSurface *layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
    Q_ASSERT(layer);

    auto validGeo = layer->exclusiveZone() == -1 ? this->rect() : validRect();
    validGeo = validGeo.marginsRemoved(QMargins(layer->leftMargin(),
                                                layer->topMargin(),
                                                layer->rightMargin(),
                                                layer->bottomMargin()));
    auto anchor = layer->ancher();
    QRectF surfaceGeo(QPointF(0, 0), layer->desiredSize());

    if (anchor.testFlags(WLayerSurface::AnchorType::Left | WLayerSurface::AnchorType::Right)) {
        surfaceGeo.moveLeft(validGeo.left());
        surfaceGeo.setWidth(validGeo.width());
    } else if (anchor & WLayerSurface::AnchorType::Left) {
        surfaceGeo.moveLeft(validGeo.left());
    } else if (anchor & WLayerSurface::AnchorType::Right) {
        surfaceGeo.moveRight(validGeo.right());
    } else {
        surfaceGeo.moveLeft(validGeo.left() + (validGeo.width() - surfaceGeo.width()) / 2);
    }

    if (anchor.testFlags(WLayerSurface::AnchorType::Top | WLayerSurface::AnchorType::Bottom)) {
        surfaceGeo.moveTop(validGeo.top());
        surfaceGeo.setHeight(validGeo.height());
    } else if (anchor & WLayerSurface::AnchorType::Top) {
        surfaceGeo.moveTop(validGeo.top());
    } else if (anchor & WLayerSurface::AnchorType::Bottom) {
        surfaceGeo.moveBottom(validGeo.bottom());
    } else {
        surfaceGeo.moveTop(validGeo.top() + (validGeo.height() - surfaceGeo.height()) / 2);
    }

    if (layer->exclusiveZone() > 0) {
        // TODO:: support `set_exclusive_edge` in layer-shell v5, need wlroots 0.19
        switch (layer->getExclusiveZoneEdge()) {
            using enum WLayerSurface::AnchorType;
        case Top:
            setExclusiveZone(Qt::TopEdge, layer, layer->exclusiveZone());
            break;
        case Bottom:
            setExclusiveZone(Qt::BottomEdge, layer, layer->exclusiveZone());
            break;
        case Left:
            setExclusiveZone(Qt::LeftEdge, layer, layer->exclusiveZone());
            break;
        case Right:
            setExclusiveZone(Qt::RightEdge, layer, layer->exclusiveZone());
            break;
        default:
            qCWarning(qLcLayerShell)
                << layer->appId() << " has set exclusive zone, but exclusive edge is invalid!";
            break;
        }
    }

    surface->setSize(surfaceGeo.size());
    surface->setPosition(surfaceGeo.topLeft());
}

void Output::layoutLayerSurfaces()
{
    auto oldExclusiveZone = m_exclusiveZone;

    for (auto *s : surfaces()) {
        if (s->type() != SurfaceWrapper::Type::Layer)
            continue;
        removeExclusiveZone(s->shellSurface());
    }

    for (auto *s : surfaces()) {
        if (s->type() != SurfaceWrapper::Type::Layer)
            continue;
        layoutLayerSurface(s);
    }

    if (oldExclusiveZone != m_exclusiveZone) {
        layoutNonLayerSurfaces();
        emit exclusiveZoneChanged();
    }
}

void Output::layoutNonLayerSurface(SurfaceWrapper *surface, const QSizeF &sizeDiff)
{
    Q_ASSERT(surface->type() != SurfaceWrapper::Type::Layer);
    surface->setFullscreenGeometry(geometry());
    const auto validGeo = this->validGeometry();
    surface->setMaximizedGeometry(validGeo);

    QRectF normalGeo = surface->normalGeometry();
    do {
        if (surface->positionAutomatic()) {
            if (normalGeo.size().isEmpty())
                return;

            SurfaceWrapper *parentSurfaceWrapper = surface->parentSurface();
            if (parentSurfaceWrapper) {
                auto xdgSurface = qobject_cast<WXdgSurface *>(surface->shellSurface());
                auto inputPopupSurface =
                    qobject_cast<WInputPopupSurface *>(surface->shellSurface());
                if ((xdgSurface && xdgSurface->isPopup()) || inputPopupSurface) {
                    QPointF dPos = xdgSurface ? xdgSurface->getPopupPosition()
                                              : inputPopupSurface->cursorRect().topLeft();
                    QPointF topLeft;
                    // TODO: remove parentSurfaceWrapper->surfaceItem()->x()
                    topLeft.setX(parentSurfaceWrapper->x()
                                 + parentSurfaceWrapper->surfaceItem()->x() + dPos.x());
                    topLeft.setY(parentSurfaceWrapper->y()
                                 + parentSurfaceWrapper->surfaceItem()->y() + dPos.y());
                    auto output = surface->ownsOutput()->outputItem();

                    normalGeo.setWidth(std::min(output->width(), surface->width()));
                    normalGeo.setHeight(std::min(output->height(), surface->height()));
                    surface->setSize(normalGeo.size());

                    if (topLeft.x() + normalGeo.width() > output->x() + output->width())
                        topLeft.setX(output->x() + output->width() - normalGeo.width());
                    if (topLeft.y() + normalGeo.height() > output->y() + output->height())
                        topLeft.setY(output->y() + output->height() - normalGeo.height());
                    normalGeo.moveTopLeft(topLeft);
                    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
                } else {
                    QPointF dPos{ (parentSurfaceWrapper->width() - surface->width()) / 2,
                                  (parentSurfaceWrapper->height() - surface->height()) / 2 };
                    QPointF topLeft;
                    topLeft.setX(parentSurfaceWrapper->x() + dPos.x());
                    topLeft.setY(parentSurfaceWrapper->y() + dPos.y());
                    normalGeo.moveTopLeft(topLeft);
                    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
                }
            } else {
                normalGeo.moveCenter(validGeo.center());
                normalGeo.moveTop(qMax(normalGeo.top(), validGeo.top()));
                normalGeo.moveLeft(qMax(normalGeo.left(), validGeo.left()));
                surface->moveNormalGeometryInOutput(normalGeo.topLeft());
            }
        } else if (!sizeDiff.isNull() && sizeDiff.isValid()) {
            const QSizeF outputSize = m_item->size();
            const auto xScale = outputSize.width() / (outputSize.width() - sizeDiff.width());
            const auto yScale = outputSize.height() / (outputSize.height() - sizeDiff.height());
            normalGeo.moveLeft(normalGeo.x() * xScale);
            normalGeo.moveTop(normalGeo.y() * yScale);
            surface->moveNormalGeometryInOutput(normalGeo.topLeft());
        } else {
            break;
        }
    } while (false);
}

void Output::layoutNonLayerSurfaces()
{
    const auto currentSize = validRect().size();
    const auto sizeDiff = m_lastSizeOnLayoutNonLayerSurfaces.isValid()
        ? currentSize - m_lastSizeOnLayoutNonLayerSurfaces
        : QSizeF(0, 0);
    m_lastSizeOnLayoutNonLayerSurfaces = currentSize;

    for (SurfaceWrapper *surface : surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::Layer)
            continue;
        layoutNonLayerSurface(surface, sizeDiff);
    }
}

void Output::layoutAllSurfaces()
{
    layoutLayerSurfaces();
    layoutNonLayerSurfaces();
}

QMargins Output::exclusiveZone() const
{
    return m_exclusiveZone;
}

QRectF Output::rect() const
{
    return QRectF(QPointF(0, 0), m_item->size());
}

QRectF Output::geometry() const
{
    return QRectF(m_item->position(), m_item->size());
}

QRectF Output::validRect() const
{
    return rect().marginsRemoved(m_exclusiveZone);
}

WOutputViewport *Output::screenViewport() const
{
    return m_outputViewport;
}

QRectF Output::validGeometry() const
{
    return geometry().marginsRemoved(m_exclusiveZone);
}
