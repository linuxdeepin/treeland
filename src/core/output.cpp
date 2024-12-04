// Copyright (C) 2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output.h"

#include "helper.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"
#include "treelandconfig.h"
#include "workspace.h"

#include <wcursor.h>
#include <winputpopupsurface.h>
#include <wlayersurface.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <woutputrenderwindow.h>
#include <wquicktextureproxy.h>
#include <wsurfaceitem.h>
#include <wxdgpopupsurface.h>
#include <wxdgpopupsurfaceitem.h>

#include <qwlayershellv1.h>
#include <qwoutputlayout.h>

#include <QQmlEngine>

#define SAME_APP_OFFSET_FACTOR 1.0
#define DIFF_APP_OFFSET_FACTOR 2.0

Q_LOGGING_CATEGORY(qLcOutput, "treeland.output")

Output *Output::create(WOutput *output, QQmlEngine *engine, QObject *parent)
{
    auto isSoftwareCursor = [](WOutput *output) -> bool {
        return output->handle()->is_x11() || TreelandConfig::ref().forceSoftwareCursor();
    };
    QQmlComponent delegate(engine, "Treeland", "PrimaryOutput");
    QObject *obj = delegate.beginCreate(engine->rootContext());
    delegate.setInitialProperties(obj, { { "forceSoftwareCursor", isSoftwareCursor(output) } });
    delegate.completeCreate();
    WOutputItem *outputItem = qobject_cast<WOutputItem *>(obj);
    Q_ASSERT(outputItem);
    QQmlEngine::setObjectOwnership(outputItem, QQmlEngine::CppOwnership);
    outputItem->setOutput(output);

    connect(&TreelandConfig::ref(),
            &TreelandConfig::forceSoftwareCursorChanged,
            obj,
            [obj, output, isSoftwareCursor]() {
                auto forceSoftwareCursor = isSoftwareCursor(output);
                qCInfo(qLcOutput) << "forceSoftwareCursor changed to" << forceSoftwareCursor;
                obj->setProperty("forceSoftwareCursor", forceSoftwareCursor);
            });

    auto o = new Output(outputItem, parent);
    o->m_type = Type::Primary;
    obj->setParent(o);

    o->minimizedSurfaces->setFilter([](SurfaceWrapper *s) {
        return s->isMinimized();
    });

    // Triggering layout updates using a queue helps reduce window jitter.
    // When the screen scaling factor changes, the scale of WOutput is updated first,
    // causing the size of WOutputItem to change. However, at this point, the
    // effectiveDevicePixelRatio of QWindow has not yet been updated.
    // This results in the size of maximized windows being updated prematurely.
    // Xwayland windows use the effectiveDevicePixelRatio to set the surfaceSizeRatio.
    // By updating within a queue, it ensures that the surfaceSizeRatio used when
    // resizing maximized Xwayland windows is accurate, avoiding multiple rapid
    // size changes to Xwayland windows in a short period.
    o->connect(outputItem,
               &WOutputItem::geometryChanged,
               o,
               &Output::arrangeAllSurfaces,
               Qt::QueuedConnection);

    auto contentItem = Helper::instance()->window()->contentItem();
    outputItem->setParentItem(contentItem);
    // o->m_taskBar = Helper::instance()->qmlEngine()->createTaskBar(o,
    // contentItem); o->m_taskBar->setZ(RootSurfaceContainer::TaskBarZOrder);

#ifdef QT_DEBUG
    o->m_menuBar = Helper::instance()->qmlEngine()->createMenuBar(outputItem, contentItem);
    o->m_menuBar->setZ(RootSurfaceContainer::MenuBarZOrder);
    o->setExclusiveZone(Qt::TopEdge, o->m_menuBar, o->m_menuBar->height());
#endif

    return o;
}

Output *Output::createCopy(WOutput *output, Output *proxy, QQmlEngine *engine, QObject *parent)
{
    QQmlComponent delegate(engine, "Treeland", "CopyOutput");
    QObject *obj = delegate.createWithInitialProperties(
        {
            { "targetOutputItem", QVariant::fromValue(proxy->outputItem()) },
        },
        engine->rootContext());
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
    o->updateOutputHardwareLayers();
    connect(o->m_outputViewport,
            &WOutputViewport::hardwareLayersChanged,
            o,
            &Output::updateOutputHardwareLayers);

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

#ifdef QT_DEBUG
    if (m_menuBar) {
        delete m_menuBar;
        m_menuBar = nullptr;
    }
#endif

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
    WOutputLayout *layout = output()->layout();
    Q_ASSERT(layout);
    auto *layoutOutput = layout->handle()->get(output()->nativeHandle());
    QPointF pos(layoutOutput->x, layoutOutput->y);
    m_item->setPosition(pos);
}
#ifdef QT_DEBUG
QQuickItem *Output::outputMenuBar() const
{
    return m_menuBar;
}
#endif
std::pair<WOutputViewport *, QQuickItem *> Output::getOutputItemProperty()
{
    WOutputViewport *viewportCopy =
        outputItem()->findChild<WOutputViewport *>({}, Qt::FindDirectChildrenOnly);
    Q_ASSERT(viewportCopy);
    auto textureProxy = outputItem()->findChild<WQuickTextureProxy *>();
    Q_ASSERT(textureProxy);
    return std::make_pair(viewportCopy, textureProxy);
}

void Output::placeUnderCursor(SurfaceWrapper *surface, quint32 yOffset)
{
    QSizeF cursorSize;
    QRectF normalGeo = surface->normalGeometry();
    WCursor *wCursor = Helper::instance()->seat()->cursor();
    if (!surface->ownsOutput()->outputItem()->cursorItems().isEmpty())
        cursorSize = surface->ownsOutput()->outputItem()->cursorItems()[0]->size();

    normalGeo.moveLeft(wCursor->position().x() + (cursorSize.width() - surface->width()) / 2);
    normalGeo.moveTop(wCursor->position().y() + cursorSize.height() + yOffset);
    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
}

void Output::placeClientRequstPos(SurfaceWrapper *surface, QPoint clientRequstPos)
{
    QRectF normalGeo = surface->normalGeometry();
    normalGeo.moveLeft(clientRequstPos.x());
    normalGeo.moveTop(clientRequstPos.y());
    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
}

void Output::placeCentered(SurfaceWrapper *surface)
{
    const auto validGeo = validGeometry();
    QRectF normalGeo = surface->normalGeometry();
    normalGeo.moveCenter(validGeo.center());
    normalGeo.moveTop(qMax(normalGeo.top(), validGeo.top()));
    normalGeo.moveLeft(qMax(normalGeo.left(), validGeo.left()));
    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
}

void Output::placeSmartCascaded(SurfaceWrapper *surface)
{
    auto wpModle = Helper::instance()->workspace()->modelFromId(surface->workspaceId());
    Q_ASSERT(wpModle);
    auto latestActiveSurface = wpModle->activePenultimateWindow();
    if (!latestActiveSurface || latestActiveSurface == surface) {
        placeCentered(surface);
        return;
    }

    const auto validGeo = validGeometry();
    QRectF normalGeo = surface->normalGeometry();
    QRectF latestActiveSurfaceGeo = latestActiveSurface->normalGeometry();

    qreal factor =
        (latestActiveSurface->shellSurface()->appId() != surface->shellSurface()->appId())
        ? DIFF_APP_OFFSET_FACTOR
        : SAME_APP_OFFSET_FACTOR;
    const QRectF titleBarGeometry = latestActiveSurface->titlebarGeometry();
    qreal offset = (titleBarGeometry.isNull() ? TreelandConfig::ref().windowTitlebarHeight()
                                              : titleBarGeometry.height())
        * factor;

    QPointF newPos;
    if (m_nextPlaceDirection == PlaceDirection::BottomRight) {
        newPos = calculateBottomRightPosition(latestActiveSurfaceGeo,
                                              normalGeo,
                                              validGeo,
                                              QSizeF(offset, offset));
    } else {
        newPos = calculateTopLeftPosition(latestActiveSurfaceGeo,
                                          normalGeo,
                                          validGeo,
                                          QSizeF(offset, offset));
    }

    newPos = constrainToValidArea(newPos, normalGeo.size(), validGeo);
    surface->moveNormalGeometryInOutput(newPos);
}

QPointF Output::calculateBottomRightPosition(const QRectF &activeGeo,
                                             const QRectF &normalGeo,
                                             const QRectF &validGeo,
                                             const QSizeF &offset)
{
    QPointF bottomRight(activeGeo.left() + offset.width(), activeGeo.top() + offset.height());

    if (bottomRight.x() + normalGeo.width() <= validGeo.right()
        && bottomRight.y() + normalGeo.height() <= validGeo.bottom()) {
        return bottomRight;
    }

    m_nextPlaceDirection = PlaceDirection::TopLeft;
    return QPointF(qMax(validGeo.left(), activeGeo.right() - normalGeo.width() - offset.width()),
                   qMax(validGeo.top(), activeGeo.bottom() - normalGeo.height() - offset.height()));
}

QPointF Output::calculateTopLeftPosition(const QRectF &activeGeo,
                                         const QRectF &normalGeo,
                                         const QRectF &validGeo,
                                         const QSizeF &offset)
{
    QPointF topLeft(activeGeo.right() - normalGeo.width() - offset.width(),
                    activeGeo.bottom() - normalGeo.height() - offset.height());

    if (topLeft.x() >= validGeo.left() && topLeft.y() >= validGeo.top()) {
        return topLeft;
    }

    m_nextPlaceDirection = PlaceDirection::BottomRight;
    return QPointF(qMin(validGeo.right() - normalGeo.width(), activeGeo.left() + offset.width()),
                   qMin(validGeo.bottom() - normalGeo.height(), activeGeo.top() + offset.height()));
}

QPointF Output::constrainToValidArea(const QPointF &pos,
                                     const QSizeF &windowSize,
                                     const QRectF &validGeo)
{
    QPointF newPos = pos;

    newPos.setX(qMax(newPos.x(), validGeo.left()));
    newPos.setX(qMin(newPos.x(), validGeo.right() - windowSize.width()));
    newPos.setY(qMax(newPos.y(), validGeo.top()));
    newPos.setY(qMin(newPos.y(), validGeo.bottom() - windowSize.height()));

    return newPos;
}

double Output::calcPreferredScale(double widthPx, double heightPx, double widthMm, double heightMm)
{
    if (widthPx <= 0 || heightPx <= 0 || widthMm <= 0 || heightMm <= 0) {
        return 1.0;
    }

    const double lenPx = std::hypot(widthPx, heightPx);
    const double lenMm = std::hypot(widthMm, heightMm);

    // A standard scale=1.0 display
    const double lenPxStd = std::hypot(1920.0, 1080.0);
    const double lenMmStd = std::hypot(477.0, 268.0);

    // This magic number is from startdde, I don't know why.
    const double a = 0.00158;
    // The scaling factor should be adjusted according to the size
    // of the standard screen. Even if the PPI is the same, the larger
    // the screen, the larger the scaling factor should be. Otherwise,
    // the scaling factor should be smaller, as this better aligns with
    // the visual needs of the human eye.
    const double fix = (lenMm - lenMmStd) * (lenPx / lenPxStd) * a;

    const double scaleFactor = (lenPx / lenMm) / (lenPxStd / lenMmStd) + fix;

    // Ensure step is within 0.25
    return std::round(scaleFactor * 4) / 4.0;
}

qreal Output::preferredScaleFactor(const QSize &pixelSize) const
{
    auto o = output()->handle()->handle();
    return calcPreferredScale(pixelSize.width(), pixelSize.height(), o->phys_width, o->phys_height);
}

void Output::enable()
{
    // Enable on default
    auto qwoutput = output()->handle();
    qw_output_state newState;
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frame. Because QWOutput::frame signal
    // maybe emit before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this output on render.
    if (!qwoutput->property("_Enabled").toBool()) {
        qwoutput->setProperty("_Enabled", true);

        if (!qwoutput->handle()->current_mode) {
            auto mode = qwoutput->preferred_mode();
            if (mode) {
                newState.set_mode(mode);

                // TODO: read user config
                newState.set_scale(preferredScaleFactor({ mode->width, mode->height }));
            }
        } else {
            // TODO: read user config
            newState.set_scale(preferredScaleFactor(output()->size()));
        }
        newState.set_enabled(true);
        bool ok = qwoutput->commit_state(newState);
        Q_ASSERT(ok);
    }
}

void Output::updateOutputHardwareLayers()
{
    WOutputViewport *viewportPrimary = screenViewport();
    std::pair<WOutputViewport *, QQuickItem *> copyOutput = getOutputItemProperty();
    const auto layers = viewportPrimary->hardwareLayers();
    for (auto layer : layers) {
        if (m_hardwareLayersOfPrimaryOutput.removeOne(layer))
            continue;
        Helper::instance()->window()->attach(layer,
                                             copyOutput.first,
                                             viewportPrimary,
                                             copyOutput.second);
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
                           &Output::arrangeLayerSurfaces);

        arrangeLayerSurfaces();
    } else {
        auto layoutSurface = [surface, this] {
            arrangeNonLayerSurface(surface, {});
        };

        connect(surface, &SurfaceWrapper::widthChanged, this, layoutSurface);
        connect(surface, &SurfaceWrapper::heightChanged, this, layoutSurface);
        layoutSurface();

        if (surface->type() == SurfaceWrapper::Type::XdgPopup) {
            auto xdgPopupSurfaceItem = qobject_cast<WXdgPopupSurfaceItem *>(surface->surfaceItem());
            connect(xdgPopupSurfaceItem, &WXdgPopupSurfaceItem::implicitPositionChanged, this, [surface, this] {
                // Reposition should ignore positionAutomatic
                arrangePopupSurface(surface);
            });
        }
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
        arrangeLayerSurfaces();
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

void Output::arrangeLayerSurface(SurfaceWrapper *surface)
{
    WLayerSurface *layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
    Q_ASSERT(layer);
    if (!layer->handle()->handle()->initialized) {
        return;
    }

    auto validGeo = layer->exclusiveZone() == -1 ? geometry() : validGeometry();
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
        // TODO: support set_exclusive_edge in layer-shell v5/wlroots 0.19
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
            qCWarning(qLcOutput) << layer->appId()
                                 << " has set exclusive zone, but exclusive edge is invalid!";
            break;
        }
    }

    surface->setSize(surfaceGeo.size());
    surface->setPosition(surfaceGeo.topLeft());
}

void Output::arrangeLayerSurfaces()
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
        arrangeLayerSurface(s);
    }

    if (oldExclusiveZone != m_exclusiveZone) {
        arrangeNonLayerSurfaces();
        Q_EMIT exclusiveZoneChanged();
    }
}

void Output::arrangeNonLayerSurface(SurfaceWrapper *surface, const QSizeF &sizeDiff)
{
    Q_ASSERT(surface->type() != SurfaceWrapper::Type::Layer);
    surface->setFullscreenGeometry(geometry());
    const auto validGeo = this->validGeometry();
    surface->setMaximizedGeometry(validGeo);

    QRectF normalGeo = surface->normalGeometry();
    do {
        if (surface->positionAutomatic()) {
            if (normalGeo.isEmpty())
                return;

            // NOTE: Xwayland's popup don't has parent
            SurfaceWrapper *parentSurfaceWrapper = surface->parentSurface();

            if (parentSurfaceWrapper) {
                if (surface->type() == SurfaceWrapper::Type::XdgPopup
                    || surface->type() == SurfaceWrapper::Type::InputPopup) {
                    arrangePopupSurface(surface);
                    return;
                }
                QPointF dPos{ (parentSurfaceWrapper->width() - surface->width()) / 2,
                              (parentSurfaceWrapper->height() - surface->height()) / 2 };
                QPointF topLeft;
                topLeft.setX(parentSurfaceWrapper->x() + dPos.x());
                topLeft.setY(parentSurfaceWrapper->y() + dPos.y());
                normalGeo.moveTopLeft(topLeft);
                surface->moveNormalGeometryInOutput(normalGeo.topLeft());
            } else {
                // If the window exceeds the effective screen area when it is first opened, the
                // window will be maximized.
                auto outputValidGeometry = surface->ownsOutput()->validGeometry();
                if (normalGeo.width() > outputValidGeometry.width()
                    || normalGeo.height() > outputValidGeometry.height())
                    surface->resize(outputValidGeometry.size());
                if (surface->type() == SurfaceWrapper::Type::XdgToplevel) {
                    placeSmartCascaded(surface);
                } else {
                    placeCentered(surface);
                }
            }
        } else if (!sizeDiff.isNull() && sizeDiff.isValid()) {
            const QSizeF outputSize = m_item->size();
            const auto xScale = outputSize.width() / (outputSize.width() - sizeDiff.width());
            const auto yScale = outputSize.height() / (outputSize.height() - sizeDiff.height());
            normalGeo.moveLeft(normalGeo.x() * xScale);
            normalGeo.moveTop(normalGeo.y() * yScale);
            surface->moveNormalGeometryInOutput(normalGeo.topLeft());
        } else {
            QPoint clientRequstPos = surface->clientRequstPos();
            if (!clientRequstPos.isNull()) {
                placeClientRequstPos(surface, clientRequstPos);
            }

            quint32 yOffset = surface->autoPlaceYOffset();
            if (yOffset > 0) {
                placeUnderCursor(surface, yOffset);
            }
            break;
        }
    } while (false);
}

void Output::arrangePopupSurface(SurfaceWrapper *surface)
{
    SurfaceWrapper *parentSurfaceWrapper = surface->parentSurface();
    Q_ASSERT(parentSurfaceWrapper);

    QRectF normalGeo = surface->normalGeometry();
    if (normalGeo.isEmpty())
        return;

    auto xdgPopupSurfaceItem = qobject_cast<WXdgPopupSurfaceItem *>(surface->surfaceItem());
    auto inputPopupSurface = qobject_cast<WInputPopupSurface *>(surface->shellSurface());

    QPointF dPos = xdgPopupSurfaceItem ? xdgPopupSurfaceItem->implicitPosition()
                                   : inputPopupSurface->cursorRect().bottomLeft();
    QPointF topLeft;
    // TODO: remove parentSurfaceWrapper->surfaceItem()->x()
    topLeft.setX(parentSurfaceWrapper->x() + parentSurfaceWrapper->surfaceItem()->x() + dPos.x());
    topLeft.setY(parentSurfaceWrapper->y() + parentSurfaceWrapper->surfaceItem()->y() + dPos.y()
                 + parentSurfaceWrapper->titlebarGeometry().height());

    auto output = surface->ownsOutput()->outputItem();

    normalGeo.setWidth(std::min(output->width(), surface->width()));
    normalGeo.setHeight(std::min(output->height(), surface->height()));
    surface->setSize(normalGeo.size());

    if (topLeft.x() + normalGeo.width() > output->x() + output->width())
        topLeft.setX(output->x() + output->width() - normalGeo.width());
    if (topLeft.y() + normalGeo.height() > output->y() + output->height()) {
        if (xdgPopupSurfaceItem)
            topLeft.setY(output->y() + output->height() - normalGeo.height());
        else // input popup
            topLeft.setY(topLeft.y() - inputPopupSurface->cursorRect().height()
                         - normalGeo.height());
    }
    normalGeo.moveTopLeft(topLeft);
    surface->moveNormalGeometryInOutput(normalGeo.topLeft());
}

void Output::arrangeNonLayerSurfaces()
{
    const auto currentSize = validRect().size();
    const auto sizeDiff = m_lastSizeOnLayoutNonLayerSurfaces.isValid()
        ? currentSize - m_lastSizeOnLayoutNonLayerSurfaces
        : QSizeF(0, 0);
    m_lastSizeOnLayoutNonLayerSurfaces = currentSize;

    for (SurfaceWrapper *surface : surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::Layer)
            continue;
        arrangeNonLayerSurface(surface, sizeDiff);
    }
}

void Output::arrangeAllSurfaces()
{
    arrangeLayerSurfaces();
    arrangeNonLayerSurfaces();
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
