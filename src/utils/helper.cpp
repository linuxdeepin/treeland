// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include "../modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "../modules/primary-output/outputmanagement.h"
#include "../modules/virtual-output/virtualoutputmanager.h"
#include "inputdevice.h"

#include <WBackend>
#include <WForeignToplevel>
#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <wcursorshapemanagerv1.h>
#include <winputmethodhelper.h>
#include <winputpopupsurface.h>
#include <wlayershell.h>
#include <woutputitem.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wsocket.h>
#include <wxdgdecorationmanager.h>
#include <wxdgoutput.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <qwallocator.h>
#include <qwbackend.h>
#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwinputdevice.h>
#include <qwlayershellv1.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwscreencopyv1.h>
#include <qwsubcompositor.h>
#include <qwxdgshell.h>
#include <qwxwaylandsurface.h>

#include <QAbstractListModel>
#include <QAction>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <qjsonvalue.h>
#include <qobject.h>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1

// Foolproof reserved pixels
#define LAYER_FOOLPROOF_RESERVED_PIXELS 20

#define OUTPUTS_FOOLPROOF_RESERVED_PIXELS 5

Q_LOGGING_CATEGORY(HelperDebugLog, "TreeLand.Helper.Debug", QtDebugMsg);

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

Helper::Helper(WServer *server)
    : WSeatEventFilter()
    , m_server(server)
    , m_outputLayout(new WQuickOutputLayout(this))
    , m_cursor(new WCursor(this))
    , m_seat(new WSeat())
    , m_multiTaskViewGesture(new TogglableGesture(this))
    , m_windowGesture(new TogglableGesture(this))
    , m_outputCreator(new WQmlCreator(this))
    , m_surfaceCreator(new WQmlCreator(this))
{
    m_seat->setEventFilter(this);
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputLayout);
}

void Helper::initProtocols(WOutputRenderWindow *window)
{
    QQmlApplicationEngine *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(this));

    auto backend = m_server->attach<WBackend>();
    Q_ASSERT(backend->handle());

    m_renderer = WRenderHelper::createRenderer(backend->handle());

    if (!m_renderer) {
        qFatal("Failed to create renderer");
    }

    m_allocator = qw_allocator::autocreate(*backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());

    auto *xdgShell = m_server->attach<WXdgShell>();
    auto *layerShell = m_server->attach<WLayerShell>();
    m_foreignToplevel = m_server->attach<WForeignToplevel>(xdgShell);

    m_server->attach(m_seat);
    m_seat->setKeyboardFocusWindow(window);

    m_xdgOutputManager = m_server->attach<WXdgOutputManager>(m_outputLayout);
    m_xwaylandOutputManager = m_server->attach<WXdgOutputManager>(m_outputLayout);
    m_xwaylandOutputManager->setScaleOverride(1.0);

    // default xwayland server
    auto *xwayland = createXWayland();
    setXWaylandSocket(xwayland->displayName());

    // m_xdgOutputManager->setFilter([this](WClient *client) {
    //     for (auto *xwayland : m_xwaylands) {
    //         if (client == xwayland->waylandClient()) {
    //             return false;
    //         }
    //     }
    //     return true;
    // });
    //
    // m_xwaylandOutputManager->setFilter([this](WClient *client) {
    //     for (auto *xwayland : m_xwaylands) {
    //         if (client == xwayland->waylandClient()) {
    //             return true;
    //         }
    //     }
    //     return false;
    // });

    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    Q_EMIT xdgDecorationManagerChanged();

    bool freezeClientWhenDisable = false;
    m_socket = new WSocket(freezeClientWhenDisable);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
        setWaylandSocket(m_socket->fullServerName());
    } else {
        delete m_socket;
        qCritical("Failed to create socket");
    }

    auto *outputManager = m_server->attach<WOutputManagerV1>();
    connect(outputManager,
            &WOutputManagerV1::requestTestOrApply,
            this,
            [this, outputManager](qw_output_configuration_v1 *config, bool onlyTest) {
                QList<WOutputState> states = outputManager->stateListPending();
                bool ok = true;
                for (auto state : states) {
                    WOutput *output = state.output;
                    output->enable(state.enabled);
                    if (state.enabled) {
                        if (state.mode)
                            output->setMode(state.mode);
                        else
                            output->setCustomMode(state.customModeSize, state.customModeRefresh);

                        output->enableAdaptiveSync(state.adaptiveSyncEnabled);
                        if (!onlyTest) {
                            WOutputItem *item = WOutputItem::getOutputItem(output);
                            if (item) {
                                WOutputViewport *viewport =
                                    item->property("onscreenViewport").value<WOutputViewport *>();
                                if (viewport) {
                                    viewport->rotateOutput(state.transform);
                                    viewport->setOutputScale(state.scale);
                                    viewport->setX(state.x);
                                    viewport->setY(state.y);
                                }
                            }
                        }
                    }
                }
                outputManager->sendResult(config, ok);

                updateOutputsRegion();
            });

    m_server->attach<WCursorShapeManagerV1>();
    qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);

    m_treelandForeignToplevel =
        engine->singletonInstance<ForeignToplevelV1 *>("TreeLand.Protocols", "ForeignToplevelV1");
    Q_ASSERT(m_treelandForeignToplevel);

    auto m_inputMethodHelper = new WInputMethodHelper(m_server, m_seat);
    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Added,
            this,
            [this, engine](WInputPopupSurface *inputPopup) {
                auto initProperties = engine->newObject();
                initProperties.setProperty("type", "inputPopup");
                initProperties.setProperty("wSurface", engine->toScriptValue(inputPopup));
                initProperties.setProperty("wid", engine->toScriptValue(workspaceId(engine)));
                m_surfaceCreator->add(inputPopup, initProperties);
            });

    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Removed,
            m_surfaceCreator,
            &WQmlCreator::removeByOwner);

    connect(xdgShell, &WXdgShell::surfaceAdded, this, [this, engine](WXdgSurface *surface) {
        auto initProperties = engine->newObject();
        auto type = surface->isPopup() ? "popup" : "toplevel";
        initProperties.setProperty("type", type);
        initProperties.setProperty("wSurface", engine->toScriptValue(surface));
        initProperties.setProperty("wid", engine->toScriptValue(workspaceId(engine)));

        m_surfaceCreator->add(surface, initProperties);

        if (!surface->isPopup()) {
            m_foreignToplevel->addSurface(surface);
            m_treelandForeignToplevel->add(surface);
        }
    });
    connect(xdgShell, &WXdgShell::surfaceRemoved, m_surfaceCreator, &WQmlCreator::removeByOwner);
    connect(xdgShell, &WXdgShell::surfaceRemoved, m_foreignToplevel, [this](WXdgSurface *surface) {
        if (!surface->isPopup()) {
            m_foreignToplevel->removeSurface(surface);
            m_treelandForeignToplevel->remove(surface);
        }
    });

    connect(layerShell, &WLayerShell::surfaceAdded, this, [this, engine](WLayerSurface *surface) {
        auto initProperties = engine->newObject();
        initProperties.setProperty("type", "layerShell");
        initProperties.setProperty("wSurface", engine->toScriptValue(surface));
        m_surfaceCreator->add(surface, initProperties);
    });

    connect(layerShell,
            &WLayerShell::surfaceRemoved,
            m_surfaceCreator,
            &WQmlCreator::removeByOwner);

    connect(backend,
            &WBackend::outputAdded,
            this,
            [backend, this, window, engine, outputManager](WOutput *output) {
                allowNonDrmOutputAutoChangeMode(output);

                auto initProperties = engine->newObject();
                initProperties.setProperty("waylandOutput", engine->toScriptValue(output));
                initProperties.setProperty("layout", engine->toScriptValue(outputLayout()));
                initProperties.setProperty("x",
                                           engine->toScriptValue(outputLayout()->implicitWidth()));
                m_outputCreator->add(output, initProperties);

                PrimaryOutputV1 *primaryOutput =
                    engine->singletonInstance<PrimaryOutputV1 *>("TreeLand.Protocols",
                                                                 "PrimaryOutputV1");
                Q_ASSERT(primaryOutput);

                primaryOutput->newOutput(output);

                outputManager->newOutput(output);

                VirtualOutputV1 *virtualOutputV1 =
                    engine->singletonInstance<VirtualOutputV1 *>("TreeLand.Protocols",
                                                                 "VirtualOutputV1");
                Q_ASSERT(virtualOutputV1);

                virtualOutputV1->newOutput(output);

                updateOutputsRegion();
            });

    connect(backend,
            &WBackend::outputRemoved,
            this,
            [this, engine, outputManager](WOutput *output) {
                WQuickOutputLayout *outlayout = outputLayout();
                for (auto *outputitem : outlayout->outputs()) {

                    // TODO: Screen position restoration;
                    // TODO: Continue to improve this algorithm after formulating layout rules
                    if (outputitem->output() != output) {
                        if (outputitem->x() > output->position().x()
                            && outputitem->y() == output->position().y()) {
                            outputitem->setX(outputitem->x()
                                             - WOutputItem::getOutputItem(output)->width());
                        }

                        if (outputitem->y() > output->position().y()
                            && outputitem->x() == output->position().x()) {
                            outputitem->setY(outputitem->y()
                                             - WOutputItem::getOutputItem(output)->height());
                        }
                    }
                }

                VirtualOutputV1 *virtualOutputV1 =
                    engine->singletonInstance<VirtualOutputV1 *>("TreeLand.Protocols",
                                                                 "VirtualOutputV1");
                Q_ASSERT(virtualOutputV1);

                virtualOutputV1->removeOutput(output);

                m_outputCreator->removeByOwner(output);

                PrimaryOutputV1 *primaryOutput =
                    engine->singletonInstance<PrimaryOutputV1 *>("TreeLand.Protocols",
                                                                 "PrimaryOutputV1");
                Q_ASSERT(primaryOutput);

                primaryOutput->removeOutput(output);

                outputManager->removeOutput(output);

                updateOutputsRegion();
            });

    connect(backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        m_seat->attachInputDevice(device);
        if (InputDevice::instance()->initTouchPad(device)) {
            if (m_windowGesture)
                m_windowGesture->addTouchpadSwipeGesture(SwipeGesture::Up, 3);

            if (m_multiTaskViewGesture) {
                m_multiTaskViewGesture->addTouchpadSwipeGesture(SwipeGesture::Up, 4);
                m_multiTaskViewGesture->addTouchpadSwipeGesture(SwipeGesture::Right, 4);
            }
        }
    });

    connect(backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    Q_EMIT compositorChanged();

    window->init(m_renderer, m_allocator);
    backend->handle()->start();
}

WQuickOutputLayout *Helper::outputLayout() const
{
    return m_outputLayout;
}

WSeat *Helper::seat() const
{
    return m_seat;
}

qw_compositor *Helper::compositor() const
{
    return m_compositor;
}

TogglableGesture *Helper::multiTaskViewGesture() const
{
    return m_multiTaskViewGesture;
}

TogglableGesture *Helper::windowGesture() const
{
    return m_windowGesture;
}

WCursor *Helper::cursor() const
{
    return m_seat->cursor();
}

WXdgDecorationManager *Helper::xdgDecorationManager() const
{
    return m_xdgDecorationManager;
}

WQmlCreator *Helper::outputCreator() const
{
    return m_outputCreator;
}

WQmlCreator *Helper::surfaceCreator() const
{
    return m_surfaceCreator;
}

WSurfaceItem *Helper::resizingItem() const
{
    return moveReiszeState.resizingItem;
}

void Helper::setResizingItem(WSurfaceItem *newResizingItem)
{
    if (moveReiszeState.resizingItem == newResizingItem)
        return;
    moveReiszeState.resizingItem = newResizingItem;
    emit resizingItemChanged();
}

WSurfaceItem *Helper::movingItem() const
{
    return moveReiszeState.movingItem;
}

bool Helper::registerExclusiveZone(WLayerSurface *layerSurface)
{
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return false;

    auto exclusiveZone = layerSurface->exclusiveZone();
    auto exclusiveEdge = layerSurface->getExclusiveZoneEdge();

    if (exclusiveZone <= 0 || exclusiveEdge == WLayerSurface::AnchorType::None)
        return false;

    QListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>> listIter(
        infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        if (std::get<WLayerSurface *>(listIter.next()) == layerSurface)
            return false;
    }

    infoPtr->registeredSurfaceList.append(
        std::make_tuple(layerSurface, exclusiveZone, exclusiveEdge));
    switch (exclusiveEdge) {
        using enum WLayerSurface::AnchorType;
    case Top:
        infoPtr->exclusiveMargins.top += exclusiveZone;
        Q_EMIT topExclusiveMarginChanged();
        break;
    case Bottom:
        infoPtr->exclusiveMargins.bottom += exclusiveZone;
        Q_EMIT bottomExclusiveMarginChanged();
        break;
    case Left:
        infoPtr->exclusiveMargins.left += exclusiveZone;
        Q_EMIT leftExclusiveMarginChanged();
        break;
    case Right:
        infoPtr->exclusiveMargins.right += exclusiveZone;
        Q_EMIT rightExclusiveMarginChanged();
        break;
    default:
        Q_UNREACHABLE();
    }

    updateOutputsRegion();
    return true;
}

bool Helper::unregisterExclusiveZone(WLayerSurface *layerSurface)
{
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    QMutableListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>> listIter(
        infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        auto [registeredSurface, exclusiveZone, exclusiveEdge] = listIter.next();
        if (registeredSurface == layerSurface) {
            listIter.remove();

            switch (exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                infoPtr->exclusiveMargins.top -= exclusiveZone;
                Q_EMIT topExclusiveMarginChanged();
                break;
            case Bottom:
                infoPtr->exclusiveMargins.bottom -= exclusiveZone;
                Q_EMIT bottomExclusiveMarginChanged();
                break;
            case Left:
                infoPtr->exclusiveMargins.left -= exclusiveZone;
                Q_EMIT leftExclusiveMarginChanged();
                break;
            case Right:
                infoPtr->exclusiveMargins.right -= exclusiveZone;
                Q_EMIT rightExclusiveMarginChanged();
                break;
            default:
                Q_UNREACHABLE();
            }
            updateOutputsRegion();
            return true;
        }
    }
    return false;
}

Margins Helper::getExclusiveMargins(WLayerSurface *layerSurface)
{
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    Margins margins;

    if (output) {
        QMutableListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>>
            listIter(infoPtr->registeredSurfaceList);
        while (listIter.hasNext()) {
            auto [registeredSurface, exclusiveZone, exclusiveEdge] = listIter.next();
            if (registeredSurface == layerSurface)
                break;
            switch (exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                margins.top += exclusiveZone;
                break;
            case Bottom:
                margins.bottom += exclusiveZone;
                break;
            case Left:
                margins.left += exclusiveZone;
                break;
            case Right:
                margins.right += exclusiveZone;
                break;
            default:
                Q_UNREACHABLE();
            }
        }
    }

    return margins;
}

quint32 Helper::getTopExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.top;
}

quint32 Helper::getBottomExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.bottom;
}

quint32 Helper::getLeftExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.left;
}

quint32 Helper::getRightExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.right;
}

void Helper::onSurfaceEnterOutput(WToplevelSurface *surface,
                                  WSurfaceItem *surfaceItem,
                                  WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.append(surface);
    info->surfaceItemList.append(surfaceItem);
}

void Helper::onSurfaceLeaveOutput(WToplevelSurface *surface,
                                  WSurfaceItem *surfaceItem,
                                  WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.removeOne(surface);
    info->surfaceItemList.removeOne(surfaceItem);
    // should delete OutputInfo if no surface?
}

Margins Helper::getOutputExclusiveMargins(WOutput *output)
{
    return getOutputInfo(output)->exclusiveMargins;
}

std::pair<WOutput *, OutputInfo *> Helper::getFirstOutputOfSurface(WToplevelSurface *surface)
{
    for (auto zoneInfo : m_outputExclusiveZoneInfo) {
        if (std::get<OutputInfo *>(zoneInfo)->surfaceList.contains(surface))
            return zoneInfo;
    }
    return std::make_pair(nullptr, nullptr);
}

bool Helper::selectSurfaceToActivate(WToplevelSurface *surface) const
{
    if (!surface) {
        return false;
    }

    if (surface->isMinimized()) {
        return false;
    }

    if (surface->doesNotAcceptFocus())
        return false;

    return true;
}

void Helper::setLockScreen(bool lockScreen)
{
    if (m_isLockScreen == lockScreen) {
        return;
    }

    m_isLockScreen = lockScreen;

    emit lockScreenChanged();
}

void Helper::setMovingItem(WSurfaceItem *newMovingItem)
{
    if (moveReiszeState.movingItem == newMovingItem)
        return;
    moveReiszeState.movingItem = newMovingItem;
    emit movingItemChanged();
}

void Helper::stopMoveResize()
{
    if (moveReiszeState.surface)
        moveReiszeState.surface->setResizeing(false);

    setResizingItem(nullptr);
    setMovingItem(nullptr);

    moveReiszeState.surfaceItem = nullptr;
    moveReiszeState.surface = nullptr;
    moveReiszeState.seat = nullptr;
    moveReiszeState.resizeEdgets = { 0 };
}

void Helper::startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)

    moveReiszeState.surfaceItem = shell;
    moveReiszeState.surface = surface;
    moveReiszeState.seat = seat;
    moveReiszeState.resizeEdgets = { 0 };
    moveReiszeState.surfacePosOfStartMoveResize =
        getItemGlobalPosition(moveReiszeState.surfaceItem);
    if (seat)
        moveReiszeState.cursorStartMovePosition = seat->cursor()->position();

    setMovingItem(shell);
}

void Helper::startResize(
    WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)
    Q_ASSERT(edge != 0);

    moveReiszeState.surfaceItem = shell;
    moveReiszeState.surface = surface;
    moveReiszeState.seat = seat;
    moveReiszeState.surfacePosOfStartMoveResize =
        getItemGlobalPosition(moveReiszeState.surfaceItem);
    moveReiszeState.surfaceSizeOfStartMoveResize = moveReiszeState.surfaceItem->size();
    moveReiszeState.resizeEdgets = edge;
    if (seat)
        moveReiszeState.cursorStartMovePosition = seat->cursor()->position();

    surface->setResizeing(true);
    setResizingItem(shell);
}

void Helper::cancelMoveResize(WSurfaceItem *shell)
{
    if (moveReiszeState.surfaceItem != shell)
        return;
    stopMoveResize();
}

void Helper::moveCursor(WSurfaceItem *shell, WSeat *seat)
{
    QPointF position = getItemGlobalPosition(shell);
    QSizeF size = shell->size();

    seat->setCursorPosition(
        QPointF(position.x() + size.width() + 5, position.y() + size.height() + 5));
}

WSurface *Helper::getFocusSurfaceFrom(QObject *object)
{
    auto item = WSurfaceItem::fromFocusObject(object);
    return item ? item->surface() : nullptr;
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    connect(output->handle(),
            &qw_output::notify_request_state,
            this,
            &Helper::onOutputRequeseState);
}

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger qw_output::frame signal, WOutputRenderWindow
    // needs this signal to render next frmae. Because qw_output::frame signal
    // maybe emit before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this ouptut on render.
    if (!qwoutput->property("_Enabled").toBool()) {
        qwoutput->setProperty("_Enabled", true);

        if (!qwoutput->handle()->current_mode) {
            auto mode = qwoutput->preferred_mode();
            if (mode)
                output->setMode(mode);
        }
        output->enable(true);
        bool ok = output->commit();
        Q_ASSERT(ok);
    }
}

bool Helper::doGesture(QInputEvent *event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto e = static_cast<WGestureEvent *>(event);
        switch (e->gestureType()) {
        case Qt::BeginNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    InputDevice::instance()->processSwipeCancel();
                else
                    InputDevice::instance()->processSwipeEnd();
            }
            break;
        case Qt::PanNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeUpdate(e->delta());
        case Qt::ZoomNativeGesture:
        case Qt::SmartZoomNativeGesture:
        case Qt::RotateNativeGesture:
        case Qt::SwipeNativeGesture:
        default:
            break;
        }
    }
    return false;
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    // FIXME: only one meta key
    // QEvent::ShortcutOverride QKeySequence("Meta")
    // QEvent::KeyPress QKeySequence("Meta+Meta")
    // QEvent::KeyRelease QKeySequence("Meta")
    static QFlags<MetaKeyCheck> metaKeyChecks;
    if (auto e = static_cast<QKeyEvent *>(event)) {
        if (e->type() == QKeyEvent::ShortcutOverride && e->key() == Qt::Key_Meta) {
            metaKeyChecks |= MetaKeyCheck::ShortcutOverride;
        } else if (metaKeyChecks.testFlags(MetaKeyCheck::ShortcutOverride)
                   && e->type() == QKeyEvent::KeyPress && e->modifiers() == Qt::MetaModifier
                   && e->key() == Qt::Key_Meta) {
            metaKeyChecks |= MetaKeyCheck::KeyPress;
        } else if (metaKeyChecks.testFlags(MetaKeyCheck::ShortcutOverride | MetaKeyCheck::KeyPress)
                   && e->type() == QKeyEvent::KeyRelease && e->key() == Qt::Key_Meta) {
            metaKeyChecks |= MetaKeyCheck::KeyRelease;
        } else {
            metaKeyChecks = {};
        }

        if (metaKeyChecks.testFlags(MetaKeyCheck::ShortcutOverride | MetaKeyCheck::KeyPress
                                    | MetaKeyCheck::KeyRelease)) {
            metaKeyChecks = {}; // reset
            Q_EMIT metaKeyNotify();
        }
    }
    // FIXME: end

    // Alt+Tab switcher
    // TODO: move to mid handle
    auto e = static_cast<QKeyEvent *>(event);

    switch (e->key()) {
    case Qt::Key_Alt: {
        if (m_switcherOn && event->type() == QKeyEvent::KeyRelease) {
            m_switcherOn = false;
            Q_EMIT switcherOnChanged(false);
            return false;
        }
    } break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
        if (event->type() == QEvent::KeyPress) {
            // switcher would be exclusively disabled when multitask etc is on
            if (e->modifiers().testFlag(Qt::AltModifier) && m_switcherEnabled) {
                if (e->modifiers() == Qt::AltModifier) {
                    Q_EMIT switcherChanged(Switcher::Next);
                    return true;
                } else if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
                    Q_EMIT switcherChanged(Switcher::Previous);
                    return true;
                }
            }
        }
    } break;
    case Qt::Key_Right: {
        if (event->type() == QEvent::KeyPress) {
            if (m_switcherEnabled) {
                Q_EMIT switcherActiveSwitch(Switcher::Next);
                return false;
            }
        }
    } break;
    case Qt::Key_Left: {
        if (event->type() == QEvent::KeyPress) {
            if (m_switcherEnabled) {
                Q_EMIT switcherActiveSwitch(Switcher::Previous);
                return false;
            }
        }
    } break;
    default: {
    } break;
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
            qApp->exit(-1);
            return true;
        }
    }

    if (watched) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
            seat->setKeyboardFocusWindow(watched);
        } else if (event->type() == QEvent::MouseMove && !seat->keyboardFocusWindow()) {
            // TouchMove keep focus on first window
            seat->setKeyboardFocusWindow(watched);
        }
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    doGesture(event);

    if (moveReiszeState.surfaceItem
        && (seat == moveReiszeState.seat || moveReiszeState.seat == nullptr)) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            if (ev->buttons() & Qt::LeftButton && event->type() == QEvent::MouseMove) {
                if (!m_region.isEmpty() && !m_region.contains(ev->pos())) {
                    return true;
                }
            }

            if (moveReiszeState.resizeEdgets == 0) {
                auto increment_pos = ev->globalPosition() - moveReiszeState.cursorStartMovePosition;
                auto new_pos = moveReiszeState.surfacePosOfStartMoveResize
                    + moveReiszeState.surfaceItem->parentItem()->mapFromGlobal(increment_pos);
                moveReiszeState.surfaceItem->setPosition(new_pos);
            } else {
                auto increment_pos = moveReiszeState.surfaceItem->parentItem()->mapFromGlobal(
                    ev->globalPosition() - moveReiszeState.cursorStartMovePosition);
                QRectF geo(moveReiszeState.surfacePosOfStartMoveResize,
                           moveReiszeState.surfaceSizeOfStartMoveResize);

                if (moveReiszeState.resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (moveReiszeState.resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (moveReiszeState.resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (moveReiszeState.resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                if (moveReiszeState.surfaceItem->resizeSurface(geo.size().toSize()))
                    moveReiszeState.surfaceItem->setPosition(geo.topLeft());
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            stopMoveResize();
        }
    }

    return false;
}

bool Helper::afterHandleEvent(
    WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd
        || (event->type() == QEvent::NativeGesture)) {

        if (auto e = static_cast<WGestureEvent *>(event)) {
            if (!e->isEndEvent()) {
                return false;
            }
        }

        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);
        if (auto *xdgSurface = qobject_cast<WXdgSurface *>(toplevelSurface)) {
            // TODO(waylib): popupSurface should not inherit WToplevelSurface
            if (xdgSurface->isPopup()) {
                return false;
            }
        }
        setActivateSurface(toplevelSurface);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent())
            setActivateSurface(nullptr);
    }

    return false;
}

WToplevelSurface *Helper::activatedSurface() const
{
    return m_activateSurface;
}

void Helper::setActivateSurface(WToplevelSurface *newActivate)
{
    qCDebug(HelperDebugLog) << newActivate;
    if (newActivate) {
        wl_client *client = newActivate->surface()->handle()->handle()->resource->client;
        pid_t pid;
        uid_t uid;
        gid_t gid;
        wl_client_get_credentials(client, &pid, &uid, &gid);

        QString programName;
        QFile file(QString("/proc/%1/status").arg(pid));
        if (file.open(QFile::ReadOnly)) {
            programName =
                QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"), 1, 1);
        }

        if (programName == "dde-desktop") {
            return;
        }
    }

    if (m_activateSurface == newActivate)
        return;

    if (newActivate && newActivate->doesNotAcceptFocus())
        return;

    if (m_activateSurface && m_activateSurface->isActivated()) {
        if (newActivate) {
            qCDebug(HelperDebugLog)
                << "newActivate keyboardFocusPriority " << newActivate->keyboardFocusPriority();
            if (m_activateSurface->keyboardFocusPriority() > newActivate->keyboardFocusPriority())
                return;
        } else {
            if (m_activateSurface->keyboardFocusPriority() > 0)
                return;
        }
        m_activateSurface->setActivate(false);
    }

    static QMetaObject::Connection invalidCheck;
    disconnect(invalidCheck);

    m_activateSurface = newActivate;

    qCDebug(HelperDebugLog) << "Surface: " << newActivate << " is activated";

    if (newActivate) {
        invalidCheck = connect(newActivate,
                               &WToplevelSurface::aboutToBeInvalidated,
                               this,
                               [newActivate, this] {
                                   newActivate->setActivate(false);
                                   setActivateSurface(nullptr);
                               });
        newActivate->setActivate(true);
    }

    Q_EMIT activatedSurfaceChanged();
}

void Helper::onOutputRequeseState(wlr_output_event_request_state *newState)
{
    if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
        auto output = qobject_cast<qw_output *>(sender());

        if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            const QSize size(newState->state->custom_mode.width,
                             newState->state->custom_mode.height);
            output->set_custom_mode(size.width(),
                                    size.height(),
                                    newState->state->custom_mode.refresh);
        } else {
            output->set_mode(newState->state->mode);
        }

        output->commit();
    }
}

OutputInfo *Helper::getOutputInfo(WOutput *output)
{
    for (const auto &[woutput, infoPtr] : m_outputExclusiveZoneInfo)
        if (woutput == output)
            return infoPtr;
    auto infoPtr = new OutputInfo;
    m_outputExclusiveZoneInfo.append(std::make_pair(output, infoPtr));
    return infoPtr;
}

void Helper::setCurrentUser(const QString &currentUser)
{
    if (m_currentUser != currentUser) {
        m_currentUser = currentUser;
        Q_EMIT currentUserChanged(currentUser);
    }
}

void Helper::setCurrentWorkspaceId(int currentWorkspaceId)
{
    if (m_currentWorkspaceId == currentWorkspaceId) {
        return;
    }
    m_currentWorkspaceId = currentWorkspaceId;

    Q_EMIT currentWorkspaceIdChanged();
}

QString Helper::waylandSocket() const
{
    return m_waylandSocket;
}

QString Helper::xwaylandSocket() const
{
    return m_xwaylandSocket;
}

void Helper::setWaylandSocket(const QString &socketFile)
{
    m_waylandSocket = socketFile;

    emit socketFileChanged();
}

void Helper::setXWaylandSocket(const QString &socketFile)
{
    m_xwaylandSocket = socketFile;
    emit socketFileChanged();
}

WXWayland *Helper::createXWayland()
{
    auto *xwayland = m_server->attach<WXWayland>(m_compositor, false);
    m_xwaylands.append(xwayland);
    xwayland->setSeat(m_seat);

    connect(xwayland, &WXWayland::surfaceAdded, this, [this, xwayland](WXWaylandSurface *surface) {
        QQmlApplicationEngine *engine = qobject_cast<QQmlApplicationEngine *>(qmlEngine(this));
        surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface, engine] {
            auto initProperties = engine->newObject();
            initProperties.setProperty("type", "xwayland");
            initProperties.setProperty("wSurface", engine->toScriptValue(surface));
            initProperties.setProperty("wid", engine->toScriptValue(workspaceId(engine)));

            m_surfaceCreator->add(surface, initProperties);
            m_foreignToplevel->addSurface(surface);
            m_treelandForeignToplevel->add(surface);
        });
        surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface] {
            m_surfaceCreator->removeByOwner(surface);
            m_foreignToplevel->removeSurface(surface);
            m_treelandForeignToplevel->remove(surface);
        });
    });

    return xwayland;
}

void Helper::removeXWayland(WXWayland *xwayland)
{
    m_xwaylands.removeOne(xwayland);
    xwayland->safeDeleteLater();
}

QVariant Helper::workspaceId(QQmlApplicationEngine *engine) const
{
    QObject *helper = engine->singletonInstance<QObject *>("TreeLand", "QmlHelper");

    QObject *workspaceManager = helper->property("workspaceManager").value<QObject *>();
    auto *layoutOrder = workspaceManager->property("layoutOrder").value<QAbstractListModel *>();
    QJSValue retValue;
    auto data = QMetaObject::invokeMethod(layoutOrder,
                                          "get",
                                          Qt::DirectConnection,
                                          Q_RETURN_ARG(QJSValue, retValue),
                                          Q_ARG(int, m_currentWorkspaceId));
    return retValue.toQObject()->property("wsid");
}

QString Helper::clientName(Waylib::Server::WSurface *surface) const
{
    wl_client *client = surface->handle()->handle()->resource->client;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    QString programName;
    QFile file(QString("/proc/%1/status").arg(pid));
    if (file.open(QFile::ReadOnly)) {
        programName =
            QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"), 1, 1);
    }

    qDebug() << "Program name for PID" << pid << "is" << programName;
    return programName;
}

void Helper::updateOutputsRegion()
{
    m_region = QRegion();

    WQuickOutputLayout *outlayout = outputLayout();
    for (auto *output : outlayout->outputs()) {
        Margins margins = getOutputExclusiveMargins(output->output());
        QRect rect(output->x() + OUTPUTS_FOOLPROOF_RESERVED_PIXELS,
                   output->y() + OUTPUTS_FOOLPROOF_RESERVED_PIXELS,
                   output->width() - 2 * OUTPUTS_FOOLPROOF_RESERVED_PIXELS,
                   output->height() - 2 * OUTPUTS_FOOLPROOF_RESERVED_PIXELS);

        if (margins.left) {
            rect.setLeft(output->x() + margins.left + LAYER_FOOLPROOF_RESERVED_PIXELS);
        }

        if (margins.top) {
            rect.setTop(output->y() + margins.top + LAYER_FOOLPROOF_RESERVED_PIXELS);
        }

        if (margins.right && margins.left) {
            rect.setRight(output->width() - margins.right - margins.left
                          - 2 * LAYER_FOOLPROOF_RESERVED_PIXELS);
        } else if (margins.right) {
            rect.setRight(output->width() - margins.right - LAYER_FOOLPROOF_RESERVED_PIXELS);
        }

        if (margins.bottom && margins.top) {
            rect.setHeight(output->height() - margins.bottom - margins.top
                           - 2 * LAYER_FOOLPROOF_RESERVED_PIXELS);
        } else if (margins.bottom) {
            rect.setHeight(output->height() - margins.bottom - LAYER_FOOLPROOF_RESERVED_PIXELS);
        }

        m_region += rect;
    }
}

bool Helper::isLaunchpad(WLayerSurface *surface) const
{
    if (!surface) {
        return false;
    }

    auto scope = QString(surface->handle()->handle()->scope);

    return scope == "dde-shell/launchpad";
}
