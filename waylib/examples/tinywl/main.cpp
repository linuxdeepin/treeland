// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WServer>
#include <WOutput>
#include <WSurfaceItem>
#include <wxdgsurface.h>
#include <winputpopupsurface.h>
#include <wrenderhelper.h>
#include <WBackend>
#include <wxdgshell.h>
#include <wlayershell.h>
#include <wxwayland.h>
#include <woutputitem.h>
#include <wquickcursor.h>
#include <woutputrenderwindow.h>
#include <wqmlcreator.h>
#include <winputmethodhelper.h>
#include <WForeignToplevel>
#include <WXdgOutput>
#include <wxwaylandsurface.h>
#include <woutputmanagerv1.h>
#include <wcursorshapemanagerv1.h>
#include <woutputitem.h>
#include <woutputviewport.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwlogging.h>
#include <qwallocator.h>
#include <qwrenderer.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwxwaylandsurface.h>
#include <qwlayershellv1.h>
#include <qwscreencopyv1.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwgammacontorlv1.h>
#include <qwbuffer.h>
#include <qwxdgshell.h>
#include <qwdatacontrolv1.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <QKeySequence>
#include <QQmlComponent>
#include <QVariant>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_server(new WServer(this))
    , m_outputLayout(new WQuickOutputLayout(this))
    , m_cursor(new WCursor(this))
    , m_seat(new WSeat())
    , m_outputCreator(new WQmlCreator(this))
    , m_xdgShellCreator(new WQmlCreator(this))
    , m_xwaylandCreator(new WQmlCreator(this))
    , m_layerShellCreator(new WQmlCreator(this))
    , m_inputPopupCreator(new WQmlCreator(this))
{
    m_seat->setEventFilter(this);
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputLayout);
}

Helper::~Helper()
{
    for (auto info : std::as_const(m_outputExclusiveZoneInfo)) {
        if (info.second) {
            delete info.second;
        }
    }
}

void Helper::initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine)
{
    auto backend = m_server->attach<WBackend>();
    m_server->start();
    m_renderer = WRenderHelper::createRenderer(backend->handle());

    if (!m_renderer) {
        qFatal("Failed to create renderer");
    }

    connect(backend, &WBackend::outputAdded, this, [backend, this, window, qmlEngine] (WOutput *output) {
        allowNonDrmOutputAutoChangeMode(output);

        auto initProperties = qmlEngine->newObject();
        initProperties.setProperty("waylandOutput", qmlEngine->toScriptValue(output));
        initProperties.setProperty("layout", qmlEngine->toScriptValue(outputLayout()));
        initProperties.setProperty("x", qmlEngine->toScriptValue(outputLayout()->implicitWidth()));

        m_outputCreator->add(output, initProperties);
    });

    connect(backend, &WBackend::outputRemoved, this, [this] (WOutput *output) {
        m_outputCreator->removeByOwner(output);
    });

    connect(backend, &WBackend::inputAdded, this, [this] (WInputDevice *device) {
        m_seat->attachInputDevice(device);
    });

    connect(backend, &WBackend::inputRemoved, this, [this] (WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_allocator = qw_allocator::autocreate(*backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());

    auto *xdgShell = m_server->attach<WXdgShell>();
    auto *foreignToplevel = m_server->attach<WForeignToplevel>(xdgShell);
    auto *layerShell = m_server->attach<WLayerShell>();
    m_server->attach(m_seat);

    auto *xdgOutputManager = m_server->attach<WXdgOutputManager>(m_outputLayout);
    auto *xwaylandOutputManager = m_server->attach<WXdgOutputManager>(m_outputLayout);

    xwaylandOutputManager->setScaleOverride(1.0);

    connect(xdgShell, &WXdgShell::surfaceAdded, this, [this, qmlEngine, foreignToplevel](WXdgSurface *surface) {
        auto initProperties = qmlEngine->newObject();
        initProperties.setProperty("type", surface->isPopup() ? "popup" : "toplevel");
        initProperties.setProperty("waylandSurface", qmlEngine->toScriptValue(surface));
        m_xdgShellCreator->add(surface, initProperties);

        if (!surface->isPopup()) {
            foreignToplevel->addSurface(surface);
        }
    });
    connect(xdgShell, &WXdgShell::surfaceRemoved, m_xdgShellCreator, &WQmlCreator::removeByOwner);
    connect(xdgShell,
            &WXdgShell::surfaceRemoved,
            foreignToplevel,
            [foreignToplevel](WXdgSurface *surface) {
                if (!surface->isPopup()) {
                    foreignToplevel->removeSurface(surface);
                }
            });

    auto xwayland_lazy = true;
    m_xwayland = m_server->attach<WXWayland>(m_compositor, xwayland_lazy);
    m_xwayland->setSeat(m_seat);

    xdgOutputManager->setFilter([this] (WClient *client) {
        return client != m_xwayland->waylandClient();
    });
    xwaylandOutputManager->setFilter([this] (WClient *client) {
        return client == m_xwayland->waylandClient();
    });

    connect(m_xwayland, &WXWayland::surfaceAdded, this, [this, qmlEngine, foreignToplevel] (WXWaylandSurface *surface) {
        surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface, qmlEngine, foreignToplevel] {
            auto initProperties = qmlEngine->newObject();
            initProperties.setProperty("waylandSurface", qmlEngine->toScriptValue(surface));

            m_xwaylandCreator->add(surface, initProperties);
            foreignToplevel->addSurface(surface);
        });
        surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface, foreignToplevel] {
            m_xwaylandCreator->removeByOwner(surface);
            foreignToplevel->removeSurface(surface);
        });
    });

    connect(layerShell, &WLayerShell::surfaceAdded, this, [this, qmlEngine](WLayerSurface *surface) {
        auto initProperties = qmlEngine->newObject();
        initProperties.setProperty("waylandSurface", qmlEngine->toScriptValue(surface));
        m_layerShellCreator->add(surface, initProperties);
    });

    connect(layerShell, &WLayerShell::surfaceRemoved, m_layerShellCreator, &WQmlCreator::removeByOwner);

    m_inputMethodHelper = new WInputMethodHelper(m_server, m_seat);

    connect(m_inputMethodHelper, &WInputMethodHelper::inputPopupSurfaceV2Added, this, [this, qmlEngine](WInputPopupSurface *inputPopup) {
        auto initProperties = qmlEngine->newObject();
        initProperties.setProperty("popupSurface", qmlEngine->toScriptValue(inputPopup));
        m_inputPopupCreator->add(inputPopup, initProperties);
    });

    connect(m_inputMethodHelper, &WInputMethodHelper::inputPopupSurfaceV2Removed, m_inputPopupCreator, &WQmlCreator::removeByOwner);

    Q_EMIT compositorChanged();

    window->init(m_renderer, m_allocator);
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();

    bool freezeClientWhenDisable = false;
    m_socket = new WSocket(freezeClientWhenDisable);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
    } else {
        delete m_socket;
        qCritical("Failed to create socket");
    }

    m_gammaControlManager = qw_gamma_control_manager_v1::create(*m_server->handle());
    connect(m_gammaControlManager, &qw_gamma_control_manager_v1::notify_set_gamma, this, [this]
            (wlr_gamma_control_manager_v1_set_gamma_event *event) {
        auto *qwOutput = qw_output::from(event->output);
        auto *wOutput = WOutput::fromHandle(qwOutput);
        size_t ramp_size = 0;
        uint16_t *r = nullptr, *g = nullptr, *b = nullptr;
        wlr_gamma_control_v1 *gamma_control = event->control;
        if (gamma_control) {
            ramp_size = gamma_control->ramp_size;
            r = gamma_control->table;
            g = gamma_control->table + gamma_control->ramp_size;
            b = gamma_control->table + 2 * gamma_control->ramp_size;
            if (!wOutput->setGammaLut(ramp_size, r, g, b)) {
                qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
            }
        }
    });
    m_wOutputManager = m_server->attach<WOutputManagerV1>();
    connect(m_wOutputManager, &WOutputManagerV1::requestTestOrApply, this, [this]
            (qw_output_configuration_v1 *config, bool onlyTest) {
        QList<WOutputState> states = m_wOutputManager->stateListPending();
        bool ok = true;
        for (auto state : std::as_const(states)) {
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
                        WOutputViewport *viewport = item->property("onscreenViewport").value<WOutputViewport *>();
                        if (viewport) {
                                    viewport->rotateOutput(state.transform);
                                    viewport->setOutputScale(state.scale);
                                    viewport->setX(state.x);
                                    viewport->setY(state.y);
                        }
                    }
                }
            }

            if (onlyTest)
                ok &= output->test();
            else
                ok &= output->commit();
        }
        m_wOutputManager->sendResult(config, ok);
    });

    m_cursorShapeManager = m_server->attach<WCursorShapeManagerV1>();
    m_fractionalScaleManagerV1 = qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    qw_data_control_manager_v1::create(*m_server->handle());

    backend->handle()->start();

    qInfo() << "Listing on:" << m_socket->fullServerName();
    startDemoClient(m_socket->fullServerName());
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

WQmlCreator *Helper::outputCreator() const
{
    return m_outputCreator;
}

WQmlCreator *Helper::xdgShellCreator() const
{
    return m_xdgShellCreator;
}

WQmlCreator *Helper::xwaylandCreator() const
{
    return m_xwaylandCreator;
}

WQmlCreator *Helper::layerShellCreator() const
{
    return m_layerShellCreator;
}

WQmlCreator *Helper::inputPopupCreator() const
{
    return m_inputPopupCreator;
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
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    auto exclusiveZone = layerSurface->exclusiveZone();
    auto exclusiveEdge = layerSurface->getExclusiveZoneEdge();

    if (exclusiveZone <= 0 || exclusiveEdge == WLayerSurface::AnchorType::None)
        return false;

    QListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        if (std::get<WLayerSurface*>(listIter.next()) == layerSurface)
            return false;
    }

    infoPtr->registeredSurfaceList.append(std::make_tuple(layerSurface, exclusiveZone, exclusiveEdge));
    switch(exclusiveEdge) {
        using enum WLayerSurface::AnchorType;
    case Top:
        infoPtr->m_topExclusiveMargin += exclusiveZone;
        Q_EMIT topExclusiveMarginChanged();
        break;
    case Bottom:
        infoPtr->m_bottomExclusiveMargin += exclusiveZone;
        Q_EMIT bottomExclusiveMarginChanged();
        break;
    case Left:
        infoPtr->m_leftExclusiveMargin += exclusiveZone;
        Q_EMIT leftExclusiveMarginChanged();
        break;
    case Right:
        infoPtr->m_rightExclusiveMargin += exclusiveZone;
        Q_EMIT rightExclusiveMarginChanged();
        break;
    default:
        Q_UNREACHABLE();
    }
    return true;
}

bool Helper::unregisterExclusiveZone(WLayerSurface *layerSurface)
{
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    QMutableListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        auto [ registeredSurface, exclusiveZone, exclusiveEdge ] = listIter.next();
        if (registeredSurface == layerSurface) {
            listIter.remove();

            switch(exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                infoPtr->m_topExclusiveMargin -= exclusiveZone;
                Q_EMIT topExclusiveMarginChanged();
                break;
            case Bottom:
                infoPtr->m_bottomExclusiveMargin -= exclusiveZone;
                Q_EMIT bottomExclusiveMarginChanged();
                break;
            case Left:
                infoPtr->m_leftExclusiveMargin -= exclusiveZone;
                Q_EMIT leftExclusiveMarginChanged();
                break;
            case Right:
                infoPtr->m_rightExclusiveMargin -= exclusiveZone;
                Q_EMIT rightExclusiveMarginChanged();
                break;
            default:
                Q_UNREACHABLE();
            }
            return true;
        }
    }

    return false;
}

QJSValue Helper::getExclusiveMargins(WLayerSurface *layerSurface)
{
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    QMargins margins{0, 0, 0, 0};

    if (output) {
        QMutableListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
        while (listIter.hasNext()) {
            auto [ registeredSurface, exclusiveZone, exclusiveEdge ] = listIter.next();
            if (registeredSurface == layerSurface)
                break;
            switch(exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                margins.setTop(margins.top() + exclusiveZone);
                break;
            case Bottom:
                margins.setBottom(margins.bottom() + exclusiveZone);
                break;
            case Left:
                margins.setLeft(margins.left() + exclusiveZone);
                break;
            case Right:
                margins.setRight(margins.right() + exclusiveZone);
                break;
            default:
                Q_UNREACHABLE();
            }
        }
    }

    QJSValue jsMargins = qmlEngine(this)->newObject(); // Can't use QMargins in QML
    jsMargins.setProperty("top" , margins.top());
    jsMargins.setProperty("bottom", margins.bottom());
    jsMargins.setProperty("left", margins.left());
    jsMargins.setProperty("right", margins.right());
    return jsMargins;
}

quint32 Helper::getTopExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_topExclusiveMargin;
}

quint32 Helper::getBottomExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_bottomExclusiveMargin;
}

quint32 Helper::getLeftExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_leftExclusiveMargin;
}

quint32 Helper::getRightExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_rightExclusiveMargin;
}

void Helper::onSurfaceEnterOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.append(surface);
    info->surfaceItemList.append(surfaceItem);
}

void Helper::onSurfaceLeaveOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.removeOne(surface);
    info->surfaceItemList.removeOne(surfaceItem);
    // should delete OutputInfo if no surface?
}

std::pair<WOutput*,OutputInfo*> Helper::getFirstOutputOfSurface(WToplevelSurface *surface)
{
    for (auto zoneInfo: m_outputExclusiveZoneInfo) {
        if (std::get<OutputInfo*>(zoneInfo)->surfaceList.contains(surface))
            return zoneInfo;
    }
    return std::make_pair(nullptr, nullptr);
}

void Helper::setSocketEnabled(bool newEnabled)
{
    if (m_socket)
        m_socket->setEnabled(newEnabled);
    else
        qWarning() << "Can't set enabled for empty socket!";
}

WXdgDecorationManager *Helper::xdgDecorationManager() const
{
    return m_xdgDecorationManager;
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
    moveReiszeState.resizeEdgets = {0};
}

void Helper::startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)

    moveReiszeState.surfaceItem = shell;
    moveReiszeState.surface = surface;
    moveReiszeState.seat = seat;
    moveReiszeState.resizeEdgets = {0};
    moveReiszeState.surfacePosOfStartMoveResize = getItemGlobalPosition(moveReiszeState.surfaceItem);

    setMovingItem(shell);
}

void Helper::startResize(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)
    Q_ASSERT(edge != 0);

    moveReiszeState.surfaceItem = shell;
    moveReiszeState.surface = surface;
    moveReiszeState.seat = seat;
    moveReiszeState.surfacePosOfStartMoveResize = getItemGlobalPosition(moveReiszeState.surfaceItem);
    moveReiszeState.surfaceSizeOfStartMoveResize = moveReiszeState.surfaceItem->size();
    moveReiszeState.resizeEdgets = edge;

    surface->setResizeing(true);
    setResizingItem(shell);
}

void Helper::cancelMoveResize(WSurfaceItem *shell)
{
    if (moveReiszeState.surfaceItem != shell)
        return;
    stopMoveResize();
}

bool Helper::startDemoClient(const QString &socket)
{
#ifdef START_DEMO
    QProcess waylandClientDemo;

    waylandClientDemo.setProgram(PROJECT_BINARY_DIR"/examples/animationclient/animationclient");
    waylandClientDemo.setArguments({"-platform", "wayland"});
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", socket);

    waylandClientDemo.setProcessEnvironment(env);
    return waylandClientDemo.startDetached();
#else
    return false;
#endif
}

WSurface *Helper::getFocusSurfaceFrom(QObject *object)
{
    auto item = WSurfaceItem::fromFocusObject(object);
    return item ? item->surface() : nullptr;
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    output->safeConnect(&qw_output::notify_request_state, this, &Helper::onOutputRequeseState);
}

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frmae. Because QWOutput::frame signal
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

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event) {
    // printf("???????????");
    qDebug() << "Event type:" << event->type();
    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent*>(event);
        auto modifiers = kevent->modifiers();
        auto key = kevent->key();
        printf("*******************************************************");
        if (modifiers == Qt::MetaModifier) {
            printf("here1");
            switch (key) {
            case Qt::Key_Q:
                qApp->quit();
                return true;
            case Qt::Key_H:
                printf("catch resizePane");
                emit resizePane(10, 1); // 左
                return true;
            case Qt::Key_J:
                emit resizePane(10, 4); // 下
                return true;
            case Qt::Key_K:
                emit resizePane(10, 3); // 上
                return true;
            case Qt::Key_L:
                emit resizePane(10, 2); // 右
                return true;
            case Qt::Key_1:
                // 实现一个
                printf("catch key_1");
                emit choosePane(1);
                return true;
            case Qt::Key_2:
                emit choosePane(2);
                return true;
            default:
                return false;
            }
        } else if (modifiers == Qt::ControlModifier) {
            // switch (key) {
                // case Qt::Key_Z:
                    // emit select
            // }
        }
        
        else if ((modifiers & Qt::MetaModifier) && (modifiers & Qt::AltModifier)) { // ctrl + alt
            if (key == Qt::Key_S) {
                emit swapPane();
                // selectItem1 = nullptr;
                // selectItem2 = nullptr;
                return true;
            } else if (key == Qt::Key_D) {
                emit removePane();
                // selectItem1 = nullptr;
                return true;
            } else if (key == Qt::Key_M) {

            }
        }
    }
    return false;
}



// bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event)
// {
//     if (event->type() == QEvent::KeyPress) {
//         auto kevent = static_cast<QKeyEvent*>(event);
//         if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
//             qApp->quit();
//             return true;
//         }
//     }

//     if (watched) {
//         if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
//             seat->setKeyboardFocusWindow(watched);
//         } else if (event->type() == QEvent::MouseMove && !seat->keyboardFocusWindow()) {
//             // TouchMove keep focus on first window
//             seat->setKeyboardFocusWindow(watched);
//         }
//     }

//     if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
//         seat->cursor()->setVisible(true);
//     } else if (event->type() == QEvent::TouchBegin) {
//         seat->cursor()->setVisible(false);
//     }

//     if (moveReiszeState.surfaceItem && (seat == moveReiszeState.seat || moveReiszeState.seat == nullptr)) {
//         // for move resize
//         if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
//             auto cursor = seat->cursor();
//             Q_ASSERT(cursor);
//             QMouseEvent *ev = static_cast<QMouseEvent*>(event);

//             if (moveReiszeState.resizeEdgets == 0) {
//                 auto increment_pos = ev->globalPosition() - cursor->lastPressedOrTouchDownPosition();
//                 auto new_pos = moveReiszeState.surfacePosOfStartMoveResize + moveReiszeState.surfaceItem->parentItem()->mapFromGlobal(increment_pos);
//                 moveReiszeState.surfaceItem->setPosition(new_pos);
//             } else {
//                 auto increment_pos = moveReiszeState.surfaceItem->parentItem()->mapFromGlobal(ev->globalPosition() - cursor->lastPressedOrTouchDownPosition());
//                 QRectF geo(moveReiszeState.surfacePosOfStartMoveResize, moveReiszeState.surfaceSizeOfStartMoveResize);

//                 if (moveReiszeState.resizeEdgets & Qt::LeftEdge)
//                     geo.setLeft(geo.left() + increment_pos.x());
//                 if (moveReiszeState.resizeEdgets & Qt::TopEdge)
//                     geo.setTop(geo.top() + increment_pos.y());

//                 if (moveReiszeState.resizeEdgets & Qt::RightEdge)
//                     geo.setRight(geo.right() + increment_pos.x());
//                 if (moveReiszeState.resizeEdgets & Qt::BottomEdge)
//                     geo.setBottom(geo.bottom() + increment_pos.y());

//                 if (moveReiszeState.surfaceItem->resizeSurface(geo.size().toSize()))
//                     moveReiszeState.surfaceItem->setPosition(geo.topLeft());
//             }

//             return true;
//         } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd) {
//             stopMoveResize();
//         }
//     }

//     return false;
// }

bool Helper::afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem*>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);
        if (auto *xdgSurface = qobject_cast<WXdgSurface *>(toplevelSurface)) {
            // TODO: popupSurface should not inherit WToplevelSurface
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
        if (static_cast<QSinglePointEvent*>(event)->isBeginEvent())
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
    if (m_activateSurface == newActivate)
        return;

    if (newActivate && newActivate->doesNotAcceptFocus())
        return;

    if (m_activateSurface) {
        if (newActivate) {
            if (m_activateSurface->keyboardFocusPriority() > newActivate->keyboardFocusPriority())
                return;
        } else {
            if (m_activateSurface->keyboardFocusPriority() > 0)
                return;
        }

        m_activateSurface->setActivate(false);
    }
    m_activateSurface = newActivate;
    if (newActivate)
        newActivate->setActivate(true);
    Q_EMIT activatedSurfaceChanged();
}

void Helper::onOutputRequeseState(wlr_output_event_request_state *newState)
{
    if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
        auto output = qobject_cast<qw_output*>(sender());

        if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            const QSize size(newState->state->custom_mode.width, newState->state->custom_mode.height);
            output->set_custom_mode(size.width(), size.height(), newState->state->custom_mode.refresh);
        } else {
            output->set_mode(newState->state->mode);
        }

        output->commit();
    }
}

OutputInfo* Helper::getOutputInfo(WOutput *output)
{
    for (const auto &[woutput, infoPtr]: m_outputExclusiveZoneInfo)
        if (woutput == output)
            return infoPtr;
    auto infoPtr = new OutputInfo;
    m_outputExclusiveZoneInfo.append(std::make_pair(output, infoPtr));
    return infoPtr;
}

int main(int argc, char *argv[]) {
    WRenderHelper::setupRendererBackend();
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    qw_log::init();
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QPointer<Helper> helper;
    int quitCode = 0;
    {
        QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        QGuiApplication::setQuitOnLastWindowClosed(false);
        QGuiApplication app(argc, argv);

        QQmlApplicationEngine waylandEngine;

        waylandEngine.loadFromModule("Tinywl", "Main");

        auto window = waylandEngine.rootObjects().first()->findChild<WOutputRenderWindow*>();
        Q_ASSERT(window);

        helper = waylandEngine.singletonInstance<Helper*>("Tinywl", "Helper");
        Q_ASSERT(helper);

        helper->initProtocols(window, &waylandEngine);

        quitCode = app.exec();
    }

    Q_ASSERT(!helper);
    Q_ASSERT(qw_buffer::get_objects().isEmpty());

    return quitCode;
}
