// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"
#include "surfacewrapper.h"
#include "output.h"
#include "workspace.h"
#include "qmlengine.h"
#include "surfacecontainer.h"
#include "rootsurfacecontainer.h"
#include "layersurfacecontainer.h"

#include <WServer>
#include <WOutput>
#include <WSurfaceItem>
#include <qqmlengine.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>
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
#include <woutputlayout.h>
#include <woutputviewport.h>
#include <wseat.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wlayersurface.h>
#include <wxdgdecorationmanager.h>
#include <wextforeigntoplevellistv1.h>

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
#include <qwdatacontrolv1.h>
#include <qwextdatacontrolv1.h>
#include <qwviewporter.h>
#include <qwalphamodifierv1.h>

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
#define EXT_DATA_CONTROL_MANAGER_V1_VERSION 1

Helper *Helper::m_instance = nullptr;
Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_surfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_backgroundContainer(new LayerSurfaceContainer(m_surfaceContainer))
    , m_bottomContainer(new LayerSurfaceContainer(m_surfaceContainer))
    , m_workspace(new Workspace(m_surfaceContainer))
    , m_topContainer(new LayerSurfaceContainer(m_surfaceContainer))
    , m_overlayContainer(new LayerSurfaceContainer(m_surfaceContainer))
    , m_popupContainer(new SurfaceContainer(m_surfaceContainer))
{
    setCurrentUserId(getuid());

    Q_ASSERT(!m_instance);
    m_instance = this;

    m_renderWindow->setColor(Qt::black);
    m_surfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    m_surfaceContainer->setFocusPolicy(Qt::StrongFocus);
#endif
    m_backgroundContainer->setZ(RootSurfaceContainer::BackgroundZOrder);
    m_bottomContainer->setZ(RootSurfaceContainer::BottomZOrder);
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_topContainer->setZ(RootSurfaceContainer::TopZOrder);
    m_overlayContainer->setZ(RootSurfaceContainer::OverlayZOrder);
    m_popupContainer->setZ(RootSurfaceContainer::PopupZOrder);
}

Helper::~Helper()
{
    for (auto s : m_surfaceContainer->surfaces()) {
        if (auto c = s->container())
            c->removeSurface(s);
    }

    delete m_surfaceContainer;
    Q_ASSERT(m_instance == this);
    m_instance = nullptr;
}

Helper *Helper::instance()
{
    return m_instance;
}

QmlEngine *Helper::qmlEngine() const
{
    return qobject_cast<QmlEngine*>(::qmlEngine(this));
}

WOutputRenderWindow *Helper::window() const
{
    return m_renderWindow;
}

Workspace* Helper::workspace() const
{
    return m_workspace;
}

void Helper::init()
{
    auto engine = qmlEngine();
    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_surfaceContainer->setQmlEngine(engine);

    m_surfaceContainer->init(m_server);
    m_seat = m_server->attach<WSeat>();
    m_seat->setEventFilter(this);
    m_seat->setCursor(m_surfaceContainer->cursor());
    m_seat->setKeyboardFocusWindow(m_renderWindow);

    m_backend = m_server->attach<WBackend>();
    connect(m_backend, &WBackend::inputAdded, this, [this] (WInputDevice *device) {
        m_seat->attachInputDevice(device);
    });

    connect(m_backend, &WBackend::inputRemoved, this, [this] (WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    auto wOutputManager = m_server->attach<WOutputManagerV1>();
    connect(m_backend, &WBackend::outputAdded, this, [this, wOutputManager] (WOutput *output) {
        allowNonDrmOutputAutoChangeMode(output);
        Output *o = nullptr;
        if (m_mode == OutputMode::Extension || !m_surfaceContainer->primaryOutput()) {
            o = Output::createPrimary(output, qmlEngine(), this);
            o->outputItem()->stackBefore(m_surfaceContainer);
            m_surfaceContainer->addOutput(o);
        } else if (m_mode == OutputMode::Copy) {
            o = Output::createCopy(output, m_surfaceContainer->primaryOutput(), qmlEngine(), this);
        }
        Q_ASSERT(o);

        m_outputList.append(o);
        enableOutput(output);
        wOutputManager->newOutput(output);
    });

    connect(m_backend, &WBackend::outputRemoved, this, [this, wOutputManager] (WOutput *output) {
        auto index = indexOfOutput(output);
        Q_ASSERT(index >= 0);
        const auto o = m_outputList.takeAt(index);
        wOutputManager->removeOutput(output);
        m_surfaceContainer->removeOutput(o);
        delete o;
    });

    auto *xdgShell = m_server->attach<WXdgShell>(5);
    m_foreignToplevel = m_server->attach<WForeignToplevel>(xdgShell);
    m_extForeignToplevelListV1 = m_server->attach<WExtForeignToplevelListV1>();
    auto *layerShell = m_server->attach<WLayerShell>(xdgShell);
    auto *xdgOutputManager = m_server->attach<WXdgOutputManager>(m_surfaceContainer->outputLayout());
    m_windowMenu = engine->createWindowMenu(this);

    connect(xdgShell, &WXdgShell::toplevelSurfaceAdded, this, [this] (WXdgToplevelSurface *surface) {
        auto wrapper = new SurfaceWrapper(qmlEngine(), surface, SurfaceWrapper::Type::XdgToplevel);
        m_foreignToplevel->addSurface(surface);
        m_extForeignToplevelListV1->addSurface(surface);

        wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(surface->surface())
                                 != WXdgDecorationManager::Server);

        auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
            if (wrapper->parentSurface())
                wrapper->parentSurface()->removeSubSurface(wrapper);
            if (wrapper->container())
                wrapper->container()->removeSurface(wrapper);

            if (auto parent = surface->parentSurface()) {
                auto parentWrapper = m_surfaceContainer->getSurface(parent);
                auto container = parentWrapper->container();
                Q_ASSERT(container);
                parentWrapper->addSubSurface(wrapper);
                container->addSurface(wrapper);
            } else {
                m_workspace->addSurface(wrapper);
            }
        };

        surface->safeConnect(&WXdgToplevelSurface::parentXdgSurfaceChanged, this, updateSurfaceWithParentContainer);
        updateSurfaceWithParentContainer();

        connect(wrapper, &SurfaceWrapper::requestShowWindowMenu, m_windowMenu, [this, wrapper] (QPoint pos) {
            QMetaObject::invokeMethod(m_windowMenu, "showWindowMenu", QVariant::fromValue(wrapper), QVariant::fromValue(pos));
        });

        Q_ASSERT(wrapper->parentItem());
    });
    connect(xdgShell, &WXdgShell::toplevelSurfaceRemoved, this, [this] (WXdgToplevelSurface *surface) {
        m_foreignToplevel->removeSurface(surface);
        m_extForeignToplevelListV1->removeSurface(surface);
        m_surfaceContainer->destroyForSurface(surface->surface());
    });

    connect(xdgShell, &WXdgShell::popupSurfaceAdded, this, [this] (WXdgPopupSurface *surface) {
        auto wrapper = new SurfaceWrapper(qmlEngine(), surface, SurfaceWrapper::Type::XdgPopup);
        wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(surface->surface())
                                 != WXdgDecorationManager::Server);
        auto parent = surface->parentSurface();
        auto parentWrapper = m_surfaceContainer->getSurface(parent);
        parentWrapper->addSubSurface(wrapper);
        m_popupContainer->addSurface(wrapper);
        wrapper->setOwnsOutput(parentWrapper->ownsOutput());

        Q_ASSERT(wrapper->parentItem());
    });
    connect(xdgShell, &WXdgShell::popupSurfaceRemoved, this, [this] (WXdgPopupSurface *surface) {
        m_surfaceContainer->destroyForSurface(surface->surface());
    });

    connect(layerShell, &WLayerShell::surfaceAdded, this, [this] (WLayerSurface *surface) {
        auto wrapper = new SurfaceWrapper(qmlEngine(), surface, SurfaceWrapper::Type::Layer);
        wrapper->setNoDecoration(true);
        updateLayerSurfaceContainer(wrapper);

        connect(surface, &WLayerSurface::layerChanged, this, [this, wrapper] {
            updateLayerSurfaceContainer(wrapper);
        });
        Q_ASSERT(wrapper->parentItem());
    });

    connect(layerShell, &WLayerShell::surfaceRemoved, this, [this] (WLayerSurface *surface) {
        m_surfaceContainer->destroyForSurface(surface->surface());
    });

    m_server->start();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qFatal("Failed to create renderer");
    }

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());
    qw_viewporter::create(*m_server->handle());
    m_renderWindow->init(m_renderer, m_allocator);

    // for xwayland
    auto *xwaylandOutputManager = m_server->attach<WXdgOutputManager>(m_surfaceContainer->outputLayout());
    xwaylandOutputManager->setScaleOverride(1.0);

    auto xwayland_lazy = true;
    m_xwayland = m_server->attach<WXWayland>(m_compositor, xwayland_lazy);
    m_xwayland->setSeat(m_seat);

    xdgOutputManager->setFilter([this] (WClient *client) {
        return client != m_xwayland->waylandClient();
    });
    xwaylandOutputManager->setFilter([this] (WClient *client) {
        return client == m_xwayland->waylandClient();
    });

    connect(m_xwayland, &WXWayland::surfaceAdded, this, [this] (WXWaylandSurface *surface) {
        surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface] {
            auto wrapper = new SurfaceWrapper(qmlEngine(), surface, SurfaceWrapper::Type::XWayland);

            // Setup title and decoration
            auto xwayland = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
            auto updateDecorationTitleBar = [this, wrapper, xwayland]() {
                if (!xwayland->isBypassManager()) {
                    wrapper->setNoTitleBar(xwayland->decorationsFlags()
                                           & WXWaylandSurface::DecorationsNoTitle);
                    wrapper->setNoDecoration(xwayland->decorationsFlags()
                                             & WXWaylandSurface::DecorationsNoBorder);
                } else {
                    wrapper->setNoTitleBar(true);
                    wrapper->setNoDecoration(true);
                }
            };
            // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
            // but WXWaylandSurface will not, so must connect to `wrapper`
            connect(xwayland, &WXWaylandSurface::bypassManagerChanged, wrapper, updateDecorationTitleBar);
            connect(xwayland,
                    &WXWaylandSurface::decorationsFlagsChanged,
                    wrapper,
                    updateDecorationTitleBar);
            updateDecorationTitleBar();

            // Setup container
            auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
                if (wrapper->parentSurface())
                    wrapper->parentSurface()->removeSubSurface(wrapper);
                if (wrapper->container())
                    wrapper->container()->removeSurface(wrapper);
                auto parent = surface->parentXWaylandSurface();
                auto parentWrapper = parent ? m_surfaceContainer->getSurface(parent) : nullptr;
                // x11 surface's parent may not associate
                if (parentWrapper) {
                    auto parentWrapper = m_surfaceContainer->getSurface(parent);
                    auto container = qobject_cast<Workspace *>(parentWrapper->container());
                    Q_ASSERT(container);
                    parentWrapper->addSubSurface(wrapper);
                    container->addSurface(wrapper, parentWrapper->workspaceId());
                } else {
                    m_workspace->addSurface(wrapper);
                }
            };
            surface->safeConnect(&WXWaylandSurface::parentXWaylandSurfaceChanged,
                                 this,
                                 updateSurfaceWithParentContainer);
            updateSurfaceWithParentContainer();

            Q_ASSERT(wrapper->parentItem());
            connect(wrapper, &SurfaceWrapper::requestShowWindowMenu, m_windowMenu, [this, wrapper] (QPoint pos) {
                QMetaObject::invokeMethod(m_windowMenu, "showWindowMenu", QVariant::fromValue(wrapper), QVariant::fromValue(pos));
            });

            m_foreignToplevel->addSurface(surface);
            m_extForeignToplevelListV1->addSurface(surface);
        });
        surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface] {
            m_foreignToplevel->removeSurface(surface);
            m_extForeignToplevelListV1->removeSurface(surface);
            m_surfaceContainer->destroyForSurface(surface->surface());
        });
    });

    m_inputMethodHelper = new WInputMethodHelper(m_server, m_seat);

    connect(m_inputMethodHelper, &WInputMethodHelper::inputPopupSurfaceV2Added, this, [this](WInputPopupSurface *inputPopup) {
        auto wrapper = new SurfaceWrapper(qmlEngine(), inputPopup, SurfaceWrapper::Type::InputPopup);
        auto parent = inputPopup->parentSurface();;
        auto parentWrapper = m_surfaceContainer->getSurface(parent);
        parentWrapper->addSubSurface(wrapper);
        m_popupContainer->addSurface(wrapper);
        wrapper->setOwnsOutput(parentWrapper->ownsOutput());
        Q_ASSERT(wrapper->parentItem());
    });

    connect(m_inputMethodHelper, &WInputMethodHelper::inputPopupSurfaceV2Removed, this, [this](WInputPopupSurface *inputPopup) {
        m_surfaceContainer->destroyForSurface(inputPopup->surface());
    });

    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager, &WXdgDecorationManager::surfaceModeChanged,
            this, [this] (WSurface *surface, WXdgDecorationManager::DecorationMode mode) {
        auto s = m_surfaceContainer->getSurface(surface);
        if (!s)
            return;
        s->setNoDecoration(mode != WXdgDecorationManager::Server);
    });

    bool freezeClientWhenDisable = false;
    m_socket = new WSocket(freezeClientWhenDisable);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
    } else {
        delete m_socket;
        qCritical("Failed to create socket");
        return;
    }

    auto gammaControlManager = qw_gamma_control_manager_v1::create(*m_server->handle());
    connect(gammaControlManager, &qw_gamma_control_manager_v1::notify_set_gamma, this, [this]
            (wlr_gamma_control_manager_v1_set_gamma_event *event) {
        auto *qwOutput = qw_output::from(event->output);
        size_t ramp_size = 0;
        uint16_t *r = nullptr, *g = nullptr, *b = nullptr;
        wlr_gamma_control_v1 *gamma_control = event->control;
        if (gamma_control) {
            ramp_size = gamma_control->ramp_size;
            r = gamma_control->table;
            g = gamma_control->table + gamma_control->ramp_size;
            b = gamma_control->table + 2 * gamma_control->ramp_size;
        }
        qw_output_state newState;
        newState.set_gamma_lut(ramp_size, r, g, b);

        if (!qwOutput->commit_state(newState)) {
            qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
        }
    });

    connect(wOutputManager, &WOutputManagerV1::requestTestOrApply, this, [this, wOutputManager]
            (qw_output_configuration_v1 *config, bool onlyTest) {
        QList<WOutputState> states = wOutputManager->stateListPending();
        bool ok = true;
        for (auto state : std::as_const(states)) {
            WOutput *output = state.output;
            qw_output_state newState;

            newState.set_enabled(state.enabled);
            if (state.enabled) {
                if (state.mode)
                    newState.set_mode(state.mode);
                else
                    newState.set_custom_mode(state.customModeSize.width(),
                                             state.customModeSize.height(),
                                             state.customModeRefresh);

                newState.set_adaptive_sync_enabled(state.adaptiveSyncEnabled);
                if (!onlyTest) {
                    newState.set_transform(static_cast<wl_output_transform>(state.transform));
                    newState.set_scale(state.scale);

                    WOutputViewport *viewport = getOutput(output)->screenViewport();
                    if (viewport) {
                        viewport->setX(state.x);
                        viewport->setY(state.y);
                    }
                }
            }

            if (onlyTest)
                ok &= output->handle()->test_state(newState);
            else
                ok &= output->handle()->commit_state(newState);
        }
        wOutputManager->sendResult(config, ok);
    });

    m_server->attach<WCursorShapeManagerV1>();
    qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    qw_data_control_manager_v1::create(*m_server->handle());
    qw_ext_data_control_manager_v1::create(*m_server->handle(), EXT_DATA_CONTROL_MANAGER_V1_VERSION);
    qw_alpha_modifier_v1::create(*m_server->handle());

    m_backend->handle()->start();

    qInfo() << "Listing on:" << m_socket->fullServerName();
    startDemoClient();
}

bool Helper::socketEnabled() const
{
    return m_socket->isEnabled();
}

void Helper::setSocketEnabled(bool newEnabled)
{
    if (m_socket)
        m_socket->setEnabled(newEnabled);
    else
        qWarning() << "Can't set enabled for empty socket!";
}

void Helper::activeSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (!wrapper || wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate))
        setActivatedSurface(wrapper);
    if (!wrapper || wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus))
        setKeyboardFocusSurface(wrapper, reason);
}

RootSurfaceContainer *Helper::rootContainer() const
{
    return m_surfaceContainer;
}

void Helper::activeSurface(SurfaceWrapper *wrapper)
{
    activeSurface(wrapper, Qt::OtherFocusReason);
}

void Helper::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_seat->setCursorPosition(position);
    Q_EMIT surface->requestResize(Qt::BottomEdge | Qt::RightEdge);
}

bool Helper::startDemoClient()
{
#ifdef START_DEMO
    QProcess waylandClientDemo;

    waylandClientDemo.setProgram(PROJECT_BINARY_DIR"/examples/animationclient/animationclient");
    waylandClientDemo.setArguments({"-platform", "wayland"});
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", m_socket->fullServerName());

    waylandClientDemo.setProcessEnvironment(env);
    return waylandClientDemo.startDetached();
#else
    return false;
#endif
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *, QInputEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent*>(event);
        if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
            Q_EMIT requestQuit();
            return true;
        } else if (event->modifiers() == Qt::MetaModifier) {
            if (kevent->key() == Qt::Key_Right) {
                m_workspace->switchToNext();
                return true;
            } else if (kevent->key() == Qt::Key_Left) {
                m_workspace->switchToPrev();
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    if (auto surface = m_surfaceContainer->moveResizeSurface()) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent*>(event);

            auto ownsOutput = surface->ownsOutput();
            if (!ownsOutput) {
                m_surfaceContainer->endMoveResize();
                return false;
            }

            auto lastPosition = m_fakelastPressedPosition.value_or(cursor->lastPressedOrTouchDownPosition());
            auto increment_pos = ev->globalPosition() - lastPosition;
            m_surfaceContainer->doMoveResize(increment_pos);

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd) {
            m_surfaceContainer->endMoveResize();
            m_fakelastPressedPosition.reset();
        }
    }

    return false;
}

bool Helper::afterHandleEvent([[maybe_unused]] WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent*>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem*>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_surfaceContainer->getSurface(watched);
        activeSurface(surface, Qt::MouseFocusReason);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent*>(event)->isBeginEvent()) {
            activeSurface(nullptr, Qt::OtherFocusReason);
        }
    }

    return false;
}

SurfaceWrapper *Helper::keyboardFocusSurface() const
{
    return m_keyboardFocusSurface;
}

void Helper::setKeyboardFocusSurface(SurfaceWrapper *newActivate, Qt::FocusReason reason)
{
    if (m_keyboardFocusSurface == newActivate)
        return;

    if (newActivate && !newActivate->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus))
        return;

    if (m_keyboardFocusSurface) {
        if (newActivate) {
            if (m_keyboardFocusSurface->shellSurface()->keyboardFocusPriority()
                > newActivate->shellSurface()->keyboardFocusPriority())
                return;
        } else {
            if (m_keyboardFocusSurface->shellSurface()->keyboardFocusPriority() > 0)
                return;
        }
    }

    if (newActivate) {
        newActivate->setFocus(true, reason);
        m_seat->setKeyboardFocusSurface(newActivate->surface());
    } else if (m_keyboardFocusSurface) {
        m_keyboardFocusSurface->setFocus(false, reason);
        m_seat->setKeyboardFocusSurface(nullptr);
    }

    m_keyboardFocusSurface = newActivate;

    Q_EMIT keyboardFocusSurfaceChanged();
}

SurfaceWrapper *Helper::activatedSurface() const
{
    return m_activatedSurface;
}

void Helper::setActivatedSurface(SurfaceWrapper *newActivateSurface)
{
    if (m_activatedSurface == newActivateSurface)
        return;

    if (newActivateSurface) {
        newActivateSurface->stackToLast();
        if (newActivateSurface->type() == SurfaceWrapper::Type::XWayland) {
            auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(newActivateSurface->shellSurface());
            xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
        }
    }

    if (m_activatedSurface)
        m_activatedSurface->setActivate(false);
    if (newActivateSurface)
        newActivateSurface->setActivate(true);
    m_activatedSurface = newActivateSurface;
    Q_EMIT activatedSurfaceChanged();
}

void Helper::setCursorPosition(const QPointF &position)
{
    m_surfaceContainer->endMoveResize();
    m_seat->setCursorPosition(position);
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    output->safeConnect(&qw_output::notify_request_state,
        this, [this] (wlr_output_event_request_state *newState) {
        if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
            auto output = qobject_cast<qw_output*>(sender());
            output->commit_state(newState->state);
        }
    });
}

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frmae. Because QWOutput::frame signal
    // maybe Q_EMIT before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this ouptut on render.
    if (!qwoutput->property("_Enabled").toBool()) {
        qwoutput->setProperty("_Enabled", true);
        qw_output_state newState;

        if (!qwoutput->handle()->current_mode) {
            auto mode = qwoutput->preferred_mode();
            if (mode)
                newState.set_mode(mode);
        }
        newState.set_enabled(true);
        bool ok = qwoutput->commit_state(newState);
        Q_ASSERT(ok);
    }
}

int Helper::indexOfOutput(WOutput *output) const
{
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i)->output() == output)
            return i;
    }
    return -1;
}

Output *Helper::getOutput(WOutput *output) const
{
    for (auto o : std::as_const(m_outputList)) {
        if (o->output() == output)
            return o;
    }
    return nullptr;
}

void Helper::addOutput()
{
    qobject_cast<qw_multi_backend*>(m_backend->handle())->for_each_backend([] (wlr_backend *backend, void *) {
        if (auto x11 = qw_x11_backend::from(backend)) {
            qw_output::from(x11->output_create());
        } else if (auto wayland = qw_wayland_backend::from(backend)) {
            qw_output::from(wayland->output_create());
        }
    }, nullptr);
}

void Helper::setOutputMode(OutputMode mode)
{
    if (m_outputList.length() < 2 || m_mode == mode)
        return;

    m_mode = mode;
    Q_EMIT outputModeChanged();
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i) == m_surfaceContainer->primaryOutput())
            continue;

        Output *o = nullptr;
        if (mode == OutputMode::Copy) {
            o = Output::createCopy(m_outputList.at(i)->output(), m_surfaceContainer->primaryOutput(), qmlEngine(), this);
            m_surfaceContainer->removeOutput(m_outputList.at(i));
        } else if(mode == OutputMode::Extension) {
            o = Output::createPrimary(m_outputList.at(i)->output(), qmlEngine(), this);
            o->outputItem()->stackBefore(m_surfaceContainer);
            m_surfaceContainer->addOutput(o);
            enableOutput(o->output());
        }
        Q_ASSERT(o);

        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i,o);
    }
}

void Helper::updateLayerSurfaceContainer(SurfaceWrapper *surface)
{
    auto layer = qobject_cast<WLayerSurface*>(surface->shellSurface());
    Q_ASSERT(layer);

    if (auto oldContainer = surface->container())
        oldContainer->removeSurface(surface);

    switch (layer->layer()) {
    case WLayerSurface::LayerType::Background:
        m_backgroundContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Bottom:
        m_bottomContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Top:
        m_topContainer->addSurface(surface);
        break;
    case WLayerSurface::LayerType::Overlay:
        m_overlayContainer->addSurface(surface);
        break;
    default:
        Q_UNREACHABLE_RETURN();
    }
}

int Helper::currentUserId() const
{
    return m_currentUserId;
}

void Helper::setCurrentUserId(int uid)
{
    if (m_currentUserId == uid)
        return;
    m_currentUserId = uid;
    Q_EMIT currentUserIdChanged();
}

float Helper::animationSpeed() const
{
    return m_animationSpeed;
}

void Helper::setAnimationSpeed(float newAnimationSpeed)
{
    if (qFuzzyCompare(m_animationSpeed, newAnimationSpeed))
        return;
    m_animationSpeed = newAnimationSpeed;
    Q_EMIT animationSpeedChanged();
}

Helper::OutputMode Helper::outputMode() const
{
    return m_mode;
}
