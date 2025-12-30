// Copyright (C) 2024-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "output.h"

#include "outputconfig.hpp"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include "cmdline.h"
#include "common/treelandlogging.h"

#include <wcursor.h>
#include <winputpopupsurface.h>
#include <wlayersurface.h>
#include <woutputitem.h>
#include <woutputhelper.h>
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
#define POPUP_EDGE_MARGIN 10

Output *Output::create(WOutput *output, QQmlEngine *engine, QObject *parent)
{
    auto isSoftwareCursor = [](WOutput *output) -> bool {
        return output->handle()->is_x11() || Helper::instance()->globalConfig()->forceSoftwareCursor();
    };
    QQmlComponent delegate(engine, "Treeland", "PrimaryOutput");
    QObject *obj = delegate.beginCreate(engine->rootContext());
    delegate.setInitialProperties(obj, { { "forceSoftwareCursor", isSoftwareCursor(output) } });
    delegate.completeCreate();
    WOutputItem *outputItem = qobject_cast<WOutputItem *>(obj);
    Q_ASSERT(outputItem);
    QQmlEngine::setObjectOwnership(outputItem, QQmlEngine::CppOwnership);
    outputItem->setOutput(output);

    connect(Helper::instance()->globalConfig(),
            &TreelandConfig::forceSoftwareCursorChanged,
            obj,
            [obj, output, isSoftwareCursor]() {
                auto forceSoftwareCursor = isSoftwareCursor(output);
                qCInfo(treelandOutput) << "forceSoftwareCursor changed to" << forceSoftwareCursor;
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

    // reset output color to config value
    o->setOutputColor(-1, 0);

    if (CmdLine::ref().enableDebugView()) {
        o->m_debugMenuBar = Helper::instance()->qmlEngine()->createMenuBar(outputItem, contentItem);
        o->m_debugMenuBar->setZ(RootSurfaceContainer::MenuBarZOrder);
#ifndef QT_DEBUG
        o->m_debugMenuBar->setVisible(false);
#else
        o->setExclusiveZone(Qt::TopEdge, o->m_debugMenuBar, o->m_debugMenuBar->height());
#endif

        connect(o->m_debugMenuBar, &QQuickItem::visibleChanged, o, [o]() {
            if (o->m_debugMenuBar->isVisible()) {
                o->setExclusiveZone(Qt::TopEdge, o->m_debugMenuBar, o->m_debugMenuBar->height());
            } else {
                o->removeExclusiveZone(o->m_debugMenuBar);
            }
        });
    }

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
    connect(proxy->screenViewport(),
            &WOutputViewport::hardwareLayersChanged,
            o,
            &Output::updateOutputHardwareLayers);

    return o;
}

Output::Output(WOutputItem *output, QObject *parent)
    : SurfaceListModel(parent)
    , m_item(output)
    , minimizedSurfaces(new SurfaceFilterModel(this))
    , m_backlight(Backlight::createForOutput(output->output()))
{
    m_outputViewport = output->property("screenViewport").value<WOutputViewport *>();

    // TODO: Investigate better ways to track the panel specific persistent settings.
    // The connector name of the panel may change.
    QString outputName = output->output()->name();
    m_config = OutputConfig::createByName("org.deepin.dde.treeland.outputs",
                                    "org.deepin.dde.treeland",
                                    "/" + outputName, this);
}

Output::~Output()
{
    if (m_taskBar) {
        delete m_taskBar;
        m_taskBar = nullptr;
    }

    if (m_debugMenuBar) {
        delete m_debugMenuBar;
        m_debugMenuBar = nullptr;
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
    WOutputLayout *layout = output()->layout();
    Q_ASSERT(layout);
    auto *layoutOutput = layout->handle()->get(output()->nativeHandle());
    QPointF pos(layoutOutput->x, layoutOutput->y);
    m_item->setPosition(pos);
}

QQuickItem *Output::debugMenuBar() const
{
    return m_debugMenuBar;
}

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
    auto wpModel = Helper::instance()->workspace()->modelFromId(surface->workspaceId());
    Q_ASSERT(wpModel);
    auto latestActiveSurface = wpModel->activePenultimateWindow();
    if (!latestActiveSurface || latestActiveSurface == surface) {
        placeCentered(surface);
        return;
    }

    const auto validGeo = validGeometry();
    QRectF normalGeo = surface->normalGeometry();
    QRectF latestActiveSurfaceGeo = latestActiveSurface->normalGeometry();

    qreal factor =
        (latestActiveSurface->appId() != surface->appId())
        ? DIFF_APP_OFFSET_FACTOR
        : SAME_APP_OFFSET_FACTOR;
    const QRectF titleBarGeometry = latestActiveSurface->titlebarGeometry();
    qreal offset = (titleBarGeometry.isNull() ? Helper::instance()->config()->windowTitlebarHeight()
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
    // maybe Q_EMIT before WOutputRenderWindow::attach, if no commit here,
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
    WOutputViewport *viewportPrimary = m_proxy->screenViewport();
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
                           &Output::arrangeAllSurfaces);

        arrangeAllSurfaces();
    } else {
        auto layoutSurface = [surface, this] {
            if (!surface->hasInitializeContainer())
                return;
            arrangeNonLayerSurface(surface, {});
        };

        connect(surface, &SurfaceWrapper::widthChanged, this, layoutSurface);
        connect(surface, &SurfaceWrapper::heightChanged, this, layoutSurface);
        connect(surface, &SurfaceWrapper::hasInitializeContainerChanged, this, layoutSurface);
        layoutSurface();

        auto setyOffset = [surface, this] {
            placeUnderCursor(surface, surface->autoPlaceYOffset());
        };
        connect(surface, &SurfaceWrapper::autoPlaceYOffsetChanged, this, setyOffset);
        if (surface->autoPlaceYOffset() != 0)
            setyOffset();

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
    clearPopupCache(surface);
    m_initialWindowPositionRatio.remove(surface);
    Q_ASSERT(hasSurface(surface));
    SurfaceListModel::removeSurface(surface);
    surface->disconnect(this);

    if (surface->type() == SurfaceWrapper::Type::Layer) {
        if (auto ss = surface->shellSurface()) {
            ss->safeDisconnect(this);
            removeExclusiveZone(ss);
        }
        arrangeAllSurfaces();
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
            qCWarning(treelandOutput) << layer->appId()
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
                if (surface->type() == SurfaceWrapper::Type::XdgToplevel
                    || surface->type() == SurfaceWrapper::Type::SplashScreen) {
                    placeSmartCascaded(surface);
                } else {
                    placeCentered(surface);
                }
            }
        } else if (!sizeDiff.isNull() && sizeDiff.isValid()) {
            QRectF validGeo = this->validGeometry();
            // Save the window's proportional position relative to the available area during the initial scale.
            if (!m_initialWindowPositionRatio.contains(surface)) {
                qreal xRatio = 0.5, yRatio = 0.5; // Default center position
                if (validGeo.width() > normalGeo.width()) {
                    xRatio = (normalGeo.x() - validGeo.x()) / (validGeo.width() - normalGeo.width());
                    xRatio = qBound(0.0, xRatio, 1.0);
                }
                if (validGeo.height() > normalGeo.height()) {
                    yRatio = (normalGeo.y() - validGeo.y()) / (validGeo.height() - normalGeo.height());
                    yRatio = qBound(0.0, yRatio, 1.0);
                }
                m_initialWindowPositionRatio[surface] = QPointF(xRatio, yRatio);
            }

            QPointF ratio = m_initialWindowPositionRatio[surface];
            QPointF newPos;
            newPos.setX(validGeo.x() + ratio.x() * (validGeo.width() - normalGeo.width()));
            newPos.setY(validGeo.y() + ratio.y() * (validGeo.height() - normalGeo.height()));

            // Boundary protection ensures that at least 30% of the window remains within the screen.
            const qreal minVisibleRatio = 0.3;
            const int minVisibleX = qMin(100, int(normalGeo.width() * minVisibleRatio));
            const int minVisibleY = qMin(100, int(normalGeo.height() * minVisibleRatio));
            newPos.setX(qBound(validGeo.left() - normalGeo.width() + minVisibleX,
                            newPos.x(),
                            validGeo.right() - minVisibleX));
            newPos.setY(qBound(validGeo.top() - normalGeo.height() + minVisibleY,
                            newPos.y(),
                            validGeo.bottom() - minVisibleY));
            surface->moveNormalGeometryInOutput(newPos);
        } else {
            QPoint clientRequstPos = surface->clientRequstPos();
            if (!clientRequstPos.isNull()) {
                placeClientRequstPos(surface, clientRequstPos);
            }

            break;
        }
    } while (false);
}

QPointF Output::calculateBasePosition(SurfaceWrapper *surface, const QPointF &dPos) const
{
    auto parent = surface->parentSurface();
    if (!parent || !parent->surfaceItem()) {
        qCWarning(treelandOutput) << " Invalid parent surface or surface item!";
        return QPointF();
    }

    return QPointF(parent->x() + parent->surfaceItem()->x() + dPos.x(),
                   parent->y() + parent->surfaceItem()->y() + dPos.y());
}

void Output::adjustToOutputBounds(QPointF &pos, const QRectF &normalGeo, const QRectF &outputRect) const
{
    if (pos.x() + normalGeo.width() > outputRect.right() - POPUP_EDGE_MARGIN) {
        pos.setX(outputRect.right() - normalGeo.width() - POPUP_EDGE_MARGIN);
    }
    if (pos.x() < outputRect.left() + POPUP_EDGE_MARGIN) {
        pos.setX(outputRect.left() + POPUP_EDGE_MARGIN);
    }

    if (pos.y() + normalGeo.height() > outputRect.bottom() - POPUP_EDGE_MARGIN) {
        pos.setY(outputRect.bottom() - normalGeo.height() - POPUP_EDGE_MARGIN);
    }
    if (pos.y() < outputRect.top() + POPUP_EDGE_MARGIN) {
        pos.setY(outputRect.top() + POPUP_EDGE_MARGIN);
    }
}

void Output::handleLayerShellPopup(SurfaceWrapper *surface, const QRectF &normalGeo)
{
    if (!surface->parentSurface() || !surface->parentSurface()->ownsOutput()) {
        qCWarning(treelandOutput) << " Invalid LayerShell parent surface!";
        return;
    }

    auto parentOutput = surface->parentSurface()->ownsOutput()->outputItem();
    auto xdgPopupSurfaceItem = qobject_cast<WXdgPopupSurfaceItem *>(surface->surfaceItem());
    auto inputPopupSurface = qobject_cast<WInputPopupSurface *>(surface->shellSurface());

    if (!xdgPopupSurfaceItem && !inputPopupSurface) {
        qCWarning(treelandOutput) << " Invalid popup surface type!";
        return;
    }

    QPointF dPos = xdgPopupSurfaceItem ? xdgPopupSurfaceItem->implicitPosition()
                                       : inputPopupSurface->cursorRect().bottomLeft();

    QPointF pos = calculateBasePosition(surface, dPos);
    if (pos.isNull()) {
        return;
    }

    QRectF outputRect(parentOutput->position(), parentOutput->size());
    adjustToOutputBounds(pos, normalGeo, outputRect);
    surface->moveNormalGeometryInOutput(pos);
}

void Output::handleRegularPopup(SurfaceWrapper *surface, const QRectF &normalGeo, bool isSubMenu, WOutputItem *targetOutput)
{
    if (normalGeo.isEmpty()) {
        return;
    }

    auto parentSurfaceWrapper = surface->parentSurface();

    auto xdgPopupSurfaceItem = qobject_cast<WXdgPopupSurfaceItem *>(surface->surfaceItem());
    auto inputPopupSurface = qobject_cast<WInputPopupSurface *>(surface->shellSurface());

    if (!xdgPopupSurfaceItem && !inputPopupSurface) {
        qCWarning(treelandOutput) << " Invalid popup surface type!";
        return;
    }

    QPointF dPos = xdgPopupSurfaceItem ? xdgPopupSurfaceItem->implicitPosition()
                                       : inputPopupSurface->cursorRect().bottomLeft();

    QPointF pos = calculateBasePosition(surface, dPos);
    if (pos.isNull()) {
        return;
    }

    QRectF outputRect(targetOutput->position(), targetOutput->size());

    if (isSubMenu) {
        pos.setX(parentSurfaceWrapper->x() + parentSurfaceWrapper->width());
        if (pos.x() + normalGeo.width() > outputRect.right()) {
            pos.setX(parentSurfaceWrapper->x() - normalGeo.width());
        }
    } else {
        if (pos.x() < outputRect.left()) {
            pos.setX(outputRect.left());
        }
        if (pos.x() + normalGeo.width() > outputRect.right()) {
            pos.setX(outputRect.right() - normalGeo.width());
        }
    }

    adjustToOutputBounds(pos, normalGeo, outputRect);

    QRectF newGeo = normalGeo;
    newGeo.moveTopLeft(pos);
    m_positionCache[surface] = qMakePair(pos, newGeo);
    surface->moveNormalGeometryInOutput(pos);
}

void Output::clearPopupCache(SurfaceWrapper *surface)
{
    if (!surface || m_positionCache.isEmpty()) {
        return;
    }

    if (surface->type() == SurfaceWrapper::Type::XdgPopup ||
        surface->type() == SurfaceWrapper::Type::InputPopup) {
        m_positionCache.remove(surface);
    }
}

void Output::arrangePopupSurface(SurfaceWrapper *surface)
{
    SurfaceWrapper *parentSurfaceWrapper = surface->parentSurface();
    Q_ASSERT(parentSurfaceWrapper);

    QRectF normalGeo = surface->normalGeometry();
    if (normalGeo.isEmpty()) {
        return;
    }

    WOutputItem* targetOutput = Helper::instance()->getOutputAtCursor()->outputItem();
    bool isSubMenu = (parentSurfaceWrapper->type() == SurfaceWrapper::Type::XdgPopup);

    if (parentSurfaceWrapper->type() == SurfaceWrapper::Type::Layer) {
        handleLayerShellPopup(surface, normalGeo);
    } else {
        handleRegularPopup(surface, normalGeo, isSubMenu, targetOutput);
    }
}

void Output::arrangeNonLayerSurfaces()
{
    const auto currentSize = validRect().size();
    const auto sizeDiff = m_lastSizeOnLayoutNonLayerSurfaces.isValid()
        ? currentSize - m_lastSizeOnLayoutNonLayerSurfaces
        : QSizeF(0, 0);
    m_lastSizeOnLayoutNonLayerSurfaces = currentSize;

    for (SurfaceWrapper *surface : surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::Layer
            || surface->type() == SurfaceWrapper::Type::LockScreen
            || !surface->hasInitializeContainer())
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

// do not use config()->setBrightness or config()->setColorTemperature to set color temperature or brightness
// as doing so will have no effect.
// use Output::setOutputColor instead
OutputConfig *Output::config() const
{
    return m_config;
}

namespace {
static inline void kelvinToRGB(double kelvin, double &r, double &g, double &b)
{
    kelvin = std::clamp(kelvin, 1000.0, 20000.0) / 100.0;

    if (kelvin <= 66.0) r = 1.0;
    else r = std::clamp(329.698727446 * std::pow(kelvin - 60.0, -0.1332047592) / 255.0, 0.0, 1.0);

    if (kelvin <= 66.0)
        g = std::clamp((99.4708025861 * std::log(kelvin) - 161.1195681661) / 255.0, 0.0, 1.0);
    else
        g = std::clamp(288.1221695283 * std::pow(kelvin - 60.0, -0.0755148492) / 255.0, 0.0, 1.0);

    if (kelvin >= 66.0) b = 1.0;
    else if (kelvin <= 19.0) b = 0.0;
    else b = std::clamp((138.5177312231 * std::log(kelvin - 10.0) - 305.0447927307) / 255.0, 0.0, 1.0);
}

static inline void generateGammaLUT(uint32_t colorTemperature,
                                    qreal brightness,
                                    size_t gammaSize,
                                    QVector<uint16_t> &r,
                                    QVector<uint16_t> &g,
                                    QVector<uint16_t> &b)
{
    if (gammaSize == 0) {
        return;
    }

    double cr, cg, cb;
    kelvinToRGB(static_cast<double>(colorTemperature), cr, cg, cb);
    for (size_t i = 0; i < gammaSize; ++i) {
        double normalized = static_cast<double>(i) / static_cast<double>(gammaSize - 1);

        double rValue = std::clamp(normalized * cr * brightness, 0.0, 1.0);
        double gValue = std::clamp(normalized * cg * brightness, 0.0, 1.0);
        double bValue = std::clamp(normalized * cb * brightness, 0.0, 1.0);
        r[i] = static_cast<uint16_t>(std::round(rValue * 65535.0));
        g[i] = static_cast<uint16_t>(std::round(gValue * 65535.0));
        b[i] = static_cast<uint16_t>(std::round(bValue * 65535.0));
    }
}

}

// TODO: better Chromatic Adaptation algorithms can be implemented when the wlr_color_transform
// api is available. For now RGB scaling is used due to limitation of gamma LUT table.
// see: http://www.brucelindbloom.com/index.html?ChromAdaptEval.html
void Output::setOutputColor(qreal brightness,
                            uint32_t colorTemperature,
                            std::function<void(bool)> resultCallback)
{
    if (brightness < 0)
        brightness = config()->brightness();
    if (colorTemperature == 0)
        colorTemperature = config()->colorTemperature();

    qreal brightnessCorrection = 1.0;

    if (m_backlight) {
        qreal backlightBrightness = m_backlight->setBrightness(brightness);
        if (backlightBrightness != 0)
            brightnessCorrection = qBound(0.0, brightness / backlightBrightness, 1.0);
    } else {
        brightnessCorrection = brightness;
    }

    const size_t gammaSize = output()->handle()->get_gamma_size();
    if (gammaSize == 0) {
        if (resultCallback)
            resultCallback(false);
        qCWarning(treelandOutput) << " Output " << output()->name()
                             << " does not support gamma LUT! Brightness and color temperature adjustments through gamma will have no effect.";
        return;
    }

    QVector<uint16_t> r(gammaSize);
    QVector<uint16_t> g(gammaSize);
    QVector<uint16_t> b(gammaSize);

    generateGammaLUT(colorTemperature,
                     brightnessCorrection,
                     gammaSize,
                     r,
                     g,
                     b);

    WOutputHelper::ExtraState newState;
    auto *viewport = screenViewport();
    auto *renderWindow = screenViewport()->outputRenderWindow();
    wlr_output_state_set_gamma_lut(newState.get(), gammaSize,
                                   r.constData(),
                                   g.constData(),
                                   b.constData());

    auto *outputHelper = renderWindow->getOutputHelper(viewport);
    outputHelper->setExtraState(newState);

    outputHelper->scheduleCommitJob([this, brightness, colorTemperature, newState, resultCallback](bool success, WOutputHelper::ExtraState state) {
        if (state == newState) {
            if (resultCallback)
                resultCallback(success);
            if (!success) {
                qCWarning(treelandOutput) << "Failed to apply brightness and color temperature settings to output"
                                          << output()->name();
            } else {
                config()->setBrightness(brightness);
                config()->setColorTemperature(colorTemperature);
            }
        } else {
            qCWarning(treelandOutput) << "Commit callback received unexpected state pointer!"
                                      << "Expected:" << newState.get()
                                      << "Got:" << state.get();
        }
    }, WOutputHelper::AfterCommitStage);
    renderWindow->update(viewport);
}
