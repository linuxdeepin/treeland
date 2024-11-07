// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include "capture.h"
#include "cmdline.h"
#include "ddeshellmanagerv1.h"
#include "inputdevice.h"
#include "layersurfacecontainer.h"
#include "lockscreen.h"
#include "output.h"
#include "outputmanagement.h"
#include "personalizationmanager.h"
#include "qmlengine.h"
#include "rootsurfacecontainer.h"
#include "shellhandler.h"
#include "shortcutmanager.h"
#include "surfacecontainer.h"
#include "surfacewrapper.h"
#include "treelandconfig.h"
#include "wallpapercolor.h"
#include "workspace.h"

#include <WBackend>
#include <WForeignToplevel>
#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <WXdgOutput>
#include <wcursorshapemanagerv1.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <qwallocator.h>
#include <qwbackend.h>
#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwdatacontrolv1.h>
#include <qwdisplay.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwgammacontorlv1.h>
#include <qwlayershellv1.h>
#include <qwlogging.h>
#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwscreencopyv1.h>
#include <qwsubcompositor.h>
#include <qwviewporter.h>
#include <qwxwaylandsurface.h>
#include <qwdatadevice.h>

#include <QAction>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QQuickWindow>

#include <pwd.h>
#include <utility>

#define WLR_FRACTIONAL_SCALE_V1_VERSION 1

Helper *Helper::m_instance = nullptr;

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_lockScreen(new LockScreen(m_rootSurfaceContainer))
    , m_windowGesture(new TogglableGesture(this))
    , m_multiTaskViewGesture(new TogglableGesture(this))
{
    setCurrentUserId(getuid());

    Q_ASSERT(!m_instance);
    m_instance = this;

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);
#endif
    m_lockScreen->setZ(RootSurfaceContainer::LockScreenZOrder);
    m_lockScreen->setVisible(false);

    connect(m_lockScreen, &LockScreen::unlock, this, [this] {
        m_currentMode = CurrentMode::Normal;

        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.0);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(1.0);
        m_workspaceOpacityAnimation->start();
    });

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer);

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow, &QQuickWindow::activeFocusItemChanged, this, [this]() {
        auto wrapper = keyboardFocusSurface();
        m_seat->setKeyboardFocusSurface(wrapper ? wrapper->surface() : nullptr);
    });

    connect(m_multiTaskViewGesture,
            &TogglableGesture::statusChanged,
            this,
            [this](TogglableGesture::Status status) {
                if (status == TogglableGesture::Activating
                    || status == TogglableGesture::Deactivating
                    || status == TogglableGesture::Active) {
                    toggleMultitaskview(Multitaskview::Gesture);
                } else {
                    if (m_multitaskview)
                        m_multitaskview->exit(nullptr);
                    m_currentMode = CurrentMode::Normal;
                }
            });

    connect(m_windowGesture, &TogglableGesture::activated, this, [this]() {
        auto surface = Helper::instance()->activatedSurface();
        if (m_currentMode == CurrentMode::Normal && surface && !surface->isMaximized()) {
            surface->requestMaximize();
        }
    });

    connect(m_windowGesture, &TogglableGesture::deactivated, this, [this]() {
        auto surface = Helper::instance()->activatedSurface();
        if (m_currentMode == CurrentMode::Normal && surface && surface->isMaximized()) {
            surface->requestCancelMaximize();
        }
    });

    connect(&TreelandConfig::ref(),
            &TreelandConfig::cursorThemeNameChanged,
            this,
            &Helper::cursorThemeChanged);
    connect(&TreelandConfig::ref(),
            &TreelandConfig::cursorSizeChanged,
            this,
            &Helper::cursorSizeChanged);
}

Helper::~Helper()
{
    for (auto s : m_rootSurfaceContainer->surfaces()) {
        if (auto c = s->container())
            c->removeSurface(s);
    }

    delete m_rootSurfaceContainer;
    Q_ASSERT(m_instance == this);
    m_instance = nullptr;
}

Helper *Helper::instance()
{
    return m_instance;
}

QmlEngine *Helper::qmlEngine() const
{
    return qobject_cast<QmlEngine *>(::qmlEngine(this));
}

WOutputRenderWindow *Helper::window() const
{
    return m_renderWindow;
}

ShellHandler *Helper::shellHandler() const
{
    return m_shellHandler;
}

Workspace *Helper::workspace() const
{
    return m_shellHandler->workspace();
}

void Helper::onOutputAdded(WOutput *output)
{
    // TODO: 应该让helper发出Output的信号，每个需要output的单元单独connect。
    allowNonDrmOutputAutoChangeMode(output);
    Output *o;
    if (m_mode == OutputMode::Extension || !m_rootSurfaceContainer->primaryOutput()) {
        o = createNormalOutput(output);
    } else if (m_mode == OutputMode::Copy) {
        o = createCopyOutput(output, m_rootSurfaceContainer->primaryOutput());
    }
    m_outputList.append(o);
    enableOutput(output);
    m_outputManager->newOutput(output);

    m_wallpaperColorV1->updateWallpaperColor(output->name(),
                                             m_personalization->backgroundIsDark(output->name()));
}

void Helper::onOutputRemoved(WOutput *output)
{
    auto index = indexOfOutput(output);
    Q_ASSERT(index >= 0);
    const auto o = m_outputList.takeAt(index);

    if (m_mode == OutputMode::Extension) {
        m_rootSurfaceContainer->removeOutput(o);
    } else if (m_mode == OutputMode::Copy) {
        m_mode = OutputMode::Extension;
        if (output == m_rootSurfaceContainer->primaryOutput()->output())
            m_rootSurfaceContainer->removeOutput(o);

        for (int i = 0; i < m_outputList.size(); i++) {
            if (m_outputList.at(i) == m_rootSurfaceContainer->primaryOutput())
                continue;
            Output *o1 = createNormalOutput(m_outputList.at(i)->output());
            enableOutput(o1->output());
            m_outputList.at(i)->deleteLater();
            m_outputList.replace(i, o1);
        }
    }

    m_outputManager->removeOutput(output);
    delete o;
}

void Helper::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_rootSurfaceContainer->getSurface(surface);
    if (!s)
        return;
    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void Helper::setGamma(struct wlr_gamma_control_manager_v1_set_gamma_event *event)
{
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
    }
    if (!wOutput->setGammaLut(ramp_size, r, g, b)) {
        qWarning() << "Failed to set gamma lut!";
        // TODO: use software impl it.
        qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
    }
}

void Helper::onOutputTestOrApply(qw_output_configuration_v1 *config, bool onlyTest)
{
    QList<WOutputState> states = m_outputManager->stateListPending();
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
                WOutputViewport *viewport = getOutput(output)->screenViewport();
                if (viewport) {
                    viewport->rotateOutput(state.transform);
                    viewport->setOutputScale(state.scale);
                    viewport->setX(state.x);
                    viewport->setY(state.y);
                }
            }
        }

        if (onlyTest)
            ok &= output->test();
        else
            ok &= output->commit();
    }
    m_outputManager->sendResult(config, ok);
}

void Helper::onDockPreview(std::vector<SurfaceWrapper *> surfaces,
                           WSurface *target,
                           QPoint pos,
                           ForeignToplevelV1::PreviewDirection direction)
{
    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);

    QMetaObject::invokeMethod(m_dockPreview,
                              "show",
                              QVariant::fromValue(surfaces),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void Helper::onDockPreviewTooltip(QString tooltip,
                                  WSurface *target,
                                  QPoint pos,
                                  ForeignToplevelV1::PreviewDirection direction)
{
    SurfaceWrapper *dockWrapper = m_rootSurfaceContainer->getSurface(target);
    Q_ASSERT(dockWrapper);
    QMetaObject::invokeMethod(m_dockPreview,
                              "showTooltip",
                              QVariant::fromValue(tooltip),
                              QVariant::fromValue(dockWrapper),
                              QVariant::fromValue(pos),
                              QVariant::fromValue(direction));
}

void Helper::onShowDesktop()
{
    WindowManagementV1::DesktopState s = m_windowManagement->desktopState();
    if (m_showDesktop == s
        || (s != WindowManagementV1::DesktopState::Normal
            && s != WindowManagementV1::DesktopState::Show))
        return;

    m_showDesktop = s;
    const auto &surfaceList = workspace()->current()->surfaces();
    for (auto surface : surfaceList) {
        if (s == WindowManagementV1::DesktopState::Normal && !surface->opacity()
            && !surface->isMinimized()) {
            surface->setOpacity(1);
            surface->startShowDesktopAnimation(true);
        } else if (s == WindowManagementV1::DesktopState::Show && surface->opacity()
                   && !surface->isMinimized()) {
            surface->setOpacity(0);
            surface->startShowDesktopAnimation(false);
        }
    }
}

void Helper::onSetCopyOutput(treeland_virtual_output_v1 *virtual_output)
{
    Output *mirrorOutput = nullptr;
    for (Output *output : m_outputList) {
        if (!virtual_output->outputList.contains(output->output()->name())) {
            QString screen = output->output()->name() + " does not exist!";
            virtual_output->send_error(TREELAND_VIRTUAL_OUTPUT_V1_ERROR_INVALID_OUTPUT,
                                       screen.toLocal8Bit().data());
            return;
        }

        if (output->output()->name() == virtual_output->outputList.at(0))
            mirrorOutput = output;
    }

    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput == mirrorOutput)
            continue;

        // When setting the primaryOutput as a copy screen, set the mirrorOutput
        // as the home screen.
        if (m_rootSurfaceContainer->primaryOutput() == currentOutput)
            m_rootSurfaceContainer->setPrimaryOutput(mirrorOutput);

        Output *o = createCopyOutput(currentOutput->output(), mirrorOutput);
        m_rootSurfaceContainer->removeOutput(currentOutput);
        currentOutput->deleteLater();
        m_outputList.replace(i, o);
    }

    m_mode = OutputMode::Copy;
}

void Helper::onRestoreCopyOutput(treeland_virtual_output_v1 *virtual_output)
{
    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput->output()->name() == virtual_output->outputList.at(0))
            continue;

        Output *o = createNormalOutput(m_outputList.at(i)->output());
        enableOutput(o->output());
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
    m_mode = OutputMode::Extension;
}

void Helper::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new PersonalizationAttached(wrapper->shellSurface(), m_personalization, wrapper);

        if (isXdgToplevel) {
            wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
            auto updateNoTitlebar = [wrapper, attached] {
                if (attached->noTitlebar()) {
                    wrapper->setNoTitleBar(true);
                } else {
                    wrapper->resetNoTitleBar();
                }
            };
            connect(attached,
                    &PersonalizationAttached::windowStateChanged,
                    wrapper,
                    updateNoTitlebar);
            updateNoTitlebar();
        } else if (isXdgPopup) {
            wrapper->setNoTitleBar(true);
            wrapper->setNoDecoration(false);
        }

        auto updateBlur = [wrapper, attached] {
            wrapper->setBlur(attached->backgroundType() == Personalization::BackgroundType::Blur);
        };
        connect(attached, &PersonalizationAttached::backgroundTypeChanged, wrapper, updateBlur);
        updateBlur();
        if (isLayer) {
            auto layer = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
            if (isLaunchpad(layer))
                wrapper->setCoverEnabled(true);
        }
    }

    if (isXwayland) {
        auto xwayland = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        auto updateDecorationTitleBar = [this, wrapper, xwayland]() {
            if (!xwayland->isBypassManager()) {
                wrapper->setNoTitleBar(xwayland->decorationsType()
                                       == WXWaylandSurface::DecorationsNoTitle);
                wrapper->setNoDecoration(xwayland->decorationsType()
                                         == WXWaylandSurface::DecorationsNoBorder);
            } else {
                wrapper->setNoTitleBar(true);
                wrapper->setNoDecoration(true);
            }
        };
        xwayland->safeConnect(&WXWaylandSurface::bypassManagerChanged,
                              this,
                              updateDecorationTitleBar);
        xwayland->safeConnect(&WXWaylandSurface::decorationsTypeChanged,
                              this,
                              updateDecorationTitleBar);
        updateDecorationTitleBar();

        if (!xwayland->isBypassManager()
            && xwayland->windowTypes() == WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_NORMAL) {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->addSurface(wrapper);
        }
    }

    if (isXdgToplevel) {
        m_foreignToplevel->addSurface(wrapper->shellSurface());
        m_treelandForeignToplevel->addSurface(wrapper);
    }

    if (!isLayer) {
        auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);
    }
}

void Helper::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (wrapper->type() == SurfaceWrapper::Type::XWayland) {
        auto xwayland = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        if (!xwayland->isBypassManager()
            && xwayland->windowTypes() == WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_NORMAL) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_treelandForeignToplevel->removeSurface(wrapper);
        }
    }
    if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel) {
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_treelandForeignToplevel->removeSurface(wrapper);
    }
}

bool Helper::surfaceBelongsToCurrentUser(SurfaceWrapper *wrapper)
{
    auto credentials = WClient::getCredentials(wrapper->surface()->waylandClient()->handle());
    return credentials->uid == currentUserId();
}

void Helper::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
    m_currentMode = CurrentMode::Normal;
}

void Helper::init()
{
    auto engine = qmlEngine();
    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);

    m_seat = m_server->attach<WSeat>();
    m_seat->setEventFilter(this);
    m_seat->setCursor(m_rootSurfaceContainer->cursor());
    m_seat->setKeyboardFocusWindow(m_renderWindow);

    connect(m_seat, &WSeat::requestDrag, this, &Helper::handleRequestDrag);

    m_backend = m_server->attach<WBackend>();
    connect(m_backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
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

    connect(m_backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_outputManager = m_server->attach<WOutputManagerV1>();
    connect(m_backend, &WBackend::outputAdded, this, &Helper::onOutputAdded);
    connect(m_backend, &WBackend::outputRemoved, this, &Helper::onOutputRemoved);

    m_ddeShellV1 = m_server->attach<DDEShellManagerV1>();
    connect(m_ddeShellV1,
            &DDEShellManagerV1::toggleMultitaskview,
            this,
            qOverload<>(&Helper::toggleMultitaskview));
    m_shellHandler->createComponent(engine);
    m_shellHandler->initXdgShell(m_server, m_ddeShellV1);
    m_shellHandler->initLayerShell(m_server);
    m_shellHandler->initInputMethodHelper(m_server, m_seat);

    m_foreignToplevel = m_server->attach<WForeignToplevel>();
    m_treelandForeignToplevel = m_server->attach<ForeignToplevelV1>();
    Q_ASSERT(m_treelandForeignToplevel);
    qmlRegisterSingletonInstance<ForeignToplevelV1>("Treeland.Protocols",
                                                    1,
                                                    0,
                                                    "ForeignToplevelV1",
                                                    m_treelandForeignToplevel);
    qRegisterMetaType<ForeignToplevelV1::PreviewDirection>();

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAdded,
            this,
            &Helper::onSurfaceWrapperAdded);

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            &Helper::onSurfaceWrapperAboutToRemove);

    auto *xdgOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());

    m_primaryOutputV1 = m_server->attach<PrimaryOutputV1>();
    m_wallpaperColorV1 = m_server->attach<WallpaperColorV1>();
    m_windowManagement = m_server->attach<WindowManagementV1>();
    m_virtualOutput = m_server->attach<VirtualOutputV1>();
    m_shortcut = m_server->attach<ShortcutV1>();
    m_server->attach<CaptureManagerV1>();
    m_personalization = m_server->attach<PersonalizationV1>();
    m_personalization->setUserId(m_currentUserId);

    connect(
        this,
        &Helper::currentUserIdChanged,
        m_personalization,
        [this]() {
            m_personalization->setUserId(m_currentUserId);
        },
        Qt::QueuedConnection);

    connect(m_personalization,
            &PersonalizationV1::backgroundChanged,
            this,
            [this](const QString &output, bool isdark) {
                m_wallpaperColorV1->updateWallpaperColor(output, isdark);
            });

    for (auto output : m_rootSurfaceContainer->outputs()) {
        const QString &outputName = output->output()->name();
        m_wallpaperColorV1->updateWallpaperColor(outputName,
                                                 m_personalization->backgroundIsDark(outputName));
    }

    connect(m_windowManagement,
            &WindowManagementV1::desktopStateChanged,
            this,
            &Helper::onShowDesktop);

    connect(m_virtualOutput,
            &VirtualOutputV1::requestCreateVirtualOutput,
            this,
            &Helper::onSetCopyOutput);

    connect(m_virtualOutput,
            &VirtualOutputV1::destroyVirtualOutput,
            this,
            &Helper::onRestoreCopyOutput);

    connect(m_primaryOutputV1,
            &PrimaryOutputV1::requestSetPrimaryOutput,
            this,
            [this](const char *name) {
                for (auto &&output : m_rootSurfaceContainer->outputs()) {
                    if (strcmp(output->output()->nativeHandle()->name, name) == 0) {
                        m_rootSurfaceContainer->setPrimaryOutput(output);
                    }
                }
            });

    connect(m_rootSurfaceContainer, &RootSurfaceContainer::primaryOutputChanged, this, [this]() {
        if (m_rootSurfaceContainer->primaryOutput()) {
            m_primaryOutputV1->sendPrimaryOutput(
                m_rootSurfaceContainer->primaryOutput()->output()->nativeHandle()->name);
        }
    });

    qmlRegisterUncreatableType<Personalization>("Treeland.Protocols",
                                                1,
                                                0,
                                                "Personalization",
                                                "Only for Enum");

    qmlRegisterUncreatableType<DDEShellHelper>("Treeland.Protocols",
                                               1,
                                               0,
                                               "DDEShellHelper",
                                               "Only for attached");
    qmlRegisterUncreatableType<CaptureSource>("Treeland.Protocols",
                                              1,
                                              0,
                                              "CaptureSource",
                                              "An abstract class");
    qmlRegisterType<CaptureContextV1>("Treeland.Protocols", 1, 0, "CaptureContextV1");
    qmlRegisterType<CaptureSourceSelector>("Treeland.Protocols", 1, 0, "CaptureSourceSelector");

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

    // for clipboard
    qw_data_control_manager_v1::create(*m_server->handle());

    // for xwayland
    auto *xwaylandOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    xwaylandOutputManager->setScaleOverride(1.0);
    m_defaultXWayland = m_shellHandler->createXWayland(m_server, m_seat, m_compositor, false);

    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &Helper::onSurfaceModeChanged);

    bool freezeClientWhenDisable = false;
    m_socket = new WSocket(freezeClientWhenDisable);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
        Q_EMIT socketFileChanged();
    } else {
        delete m_socket;
        qCritical("Failed to create socket");
        return;
    }

    auto gammaControlManager = qw_gamma_control_manager_v1::create(*m_server->handle());
    connect(gammaControlManager,
            &qw_gamma_control_manager_v1::notify_set_gamma,
            this,
            &Helper::setGamma);

    connect(m_outputManager,
            &WOutputManagerV1::requestTestOrApply,
            this,
            &Helper::onOutputTestOrApply);

    m_server->attach<WCursorShapeManagerV1>();
    qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    qw_data_control_manager_v1::create(*m_server->handle());

    m_dockPreview = engine->createDockPreview(m_renderWindow->contentItem());

    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockPreview,
            this,
            &Helper::onDockPreview);
    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockPreviewTooltip,
            this,
            &Helper::onDockPreviewTooltip);

    connect(m_treelandForeignToplevel,
            &ForeignToplevelV1::requestDockClose,
            m_dockPreview,
            [this]() {
                QMetaObject::invokeMethod(m_dockPreview, "close");
            });

    m_backend->handle()->start();

    qInfo() << "Listing on:" << m_socket->fullServerName();

    if (CmdLine::ref().useLockScreen()) {
        m_lockScreen->lock();
        m_currentMode = CurrentMode::LockScreen;

        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.4);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(0.0);
        m_workspaceOpacityAnimation->start();
    }
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

void Helper::activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (m_multitaskview && m_multitaskview->blockActiveSurface() && wrapper) {
        workspace()->pushActivedSurface(wrapper);
        return;
    }
    if (!wrapper || wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate))
        setActivatedSurface(wrapper);
    if (!wrapper || wrapper->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus))
        reuqestKeyboardFocusForSurface(wrapper, reason);
}

void Helper::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason)
{
    if (wrapper->isMinimized()) {
        wrapper->requestCancelMinimize();
    }

    if (m_showDesktop == WindowManagementV1::DesktopState::Show || !wrapper->opacity()) {
        wrapper->setOpacity(1);
        if (m_currentMode != CurrentMode::WindowSwitch)
            wrapper->startShowDesktopAnimation(true);
    }

    if (!wrapper->surface()->mapped()) {
        qWarning() << "Can't activate unmapped surface: " << wrapper;
        return;
    }

    if (!wrapper->showOnWorkspace(workspace()->current()->id()))
        workspace()->switchTo(wrapper->workspaceId());
    Helper::instance()->activateSurface(wrapper, reason);
}

RootSurfaceContainer *Helper::rootContainer() const
{
    return m_rootSurfaceContainer;
}

void Helper::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_seat->setCursorPosition(position);
    Q_EMIT surface->requestResize(Qt::BottomEdge | Qt::RightEdge);
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *, QInputEvent *event)
{
    // NOTE: Unable to distinguish meta from other key combinations
    //       For example, Meta+S will still receive Meta release after
    //       fully releasing the key, actively detect whether there are
    //       other keys, and reset the state.
    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        switch (kevent->key()) {
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
            m_singleMetaKeyPendingPressed = true;
            break;
        default:
            m_singleMetaKeyPendingPressed = false;
            break;
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
            qApp->quit();
            return true;
        } else if (event->modifiers() == Qt::MetaModifier) {
            const QList<Qt::Key> switchWorkspaceNums = { Qt::Key_1, Qt::Key_2, Qt::Key_3,
                                                         Qt::Key_4, Qt::Key_5, Qt::Key_6 };
            if (kevent->key() == Qt::Key_Right) {
                workspace()->switchToNext();
                return true;
            } else if (kevent->key() == Qt::Key_Left) {
                workspace()->switchToPrev();
                return true;
            } else if (switchWorkspaceNums.contains(kevent->key())) {
                workspace()->switchTo(switchWorkspaceNums.indexOf(kevent->key()));
                return true;
            } else if (kevent->key() == Qt::Key_S) {
                toggleMultitaskview(Multitaskview::ShortcutKey);
                return true;
            } else if (kevent->key() == Qt::Key_L) {
                if (m_lockScreen->isLocked()) {
                    return true;
                }

                m_lockScreen->lock();

                m_workspaceScaleAnimation->stop();
                m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
                m_workspaceScaleAnimation->setEndValue(1.4);
                m_workspaceScaleAnimation->start();

                m_workspaceOpacityAnimation->stop();
                m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
                m_workspaceOpacityAnimation->setEndValue(0.0);
                m_workspaceOpacityAnimation->start();

                m_currentMode = CurrentMode::LockScreen;

                return true;
            } else if (kevent->key() == Qt::Key_D) { // ShowDesktop : Meta + D
                if (m_multitaskview)
                    return true;
                if (m_showDesktop == WindowManagementV1::DesktopState::Normal)
                    m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Show);
                else if (m_showDesktop == WindowManagementV1::DesktopState::Show)
                    m_windowManagement->setDesktopState(WindowManagementV1::DesktopState::Normal);
                return true;
            } else if (kevent->key() == Qt::Key_Up && m_activatedSurface) { // maximize: Meta + up
                m_activatedSurface->requestMaximize();
                return true;
            } else if (kevent->key() == Qt::Key_Down
                       && m_activatedSurface) { // cancelMaximize : Meta + down
                m_activatedSurface->requestCancelMaximize();
                return true;
            }
        } else if (kevent->key() == Qt::Key_Alt || kevent->key() == Qt::Key_Meta) {
            if (m_taskSwitch.isNull()) {
                auto contentItem = window()->contentItem();
                auto output = rootContainer()->primaryOutput();
                m_taskSwitch = qmlEngine()->createTaskSwitcher(output, contentItem);
                connect(m_taskSwitch, SIGNAL(switchOnChanged()), this, SLOT(deleteTaskSwitch()));
                m_taskSwitch->setZ(RootSurfaceContainer::OverlayZOrder);
            }
        } else if (kevent->key() == Qt::Key_Tab || kevent->key() == Qt::Key_Backtab
                   || kevent->key() == Qt::Key_QuoteLeft || kevent->key() == Qt::Key_AsciiTilde
                   || kevent->key() == Qt::Key_Left || kevent->key() == Qt::Key_Right) {
            if (m_taskSwitch && m_taskSwitch->property("switchOn").toBool()
                && event->modifiers().testFlag(Qt::AltModifier)) {
                m_currentMode = CurrentMode::WindowSwitch;
                QString appid;
                if (kevent->key() == Qt::Key_QuoteLeft || kevent->key() == Qt::Key_AsciiTilde) {
                    auto surface =
                        m_taskSwitch->property("currentSurface").value<SurfaceWrapper *>();
                    if (surface) {
                        appid = surface->shellSurface()->appId();
                    }
                }
                auto filter = Helper::instance()->workspace()->currentFilter();
                filter->setFilterAppId(appid);

                if (kevent->key() == Qt::Key_Left) {
                    QMetaObject::invokeMethod(m_taskSwitch, "next");
                    return true;
                } else if (kevent->key() == Qt::Key_Right) {
                    QMetaObject::invokeMethod(m_taskSwitch, "previous");
                    return true;
                }

                if (event->modifiers() == Qt::AltModifier) {
                    QMetaObject::invokeMethod(m_taskSwitch, "next");
                    return true;
                } else if (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)
                           || event->modifiers()
                               == (Qt::AltModifier | Qt::MetaModifier | Qt::ShiftModifier)) {
                    QMetaObject::invokeMethod(m_taskSwitch, "previous");
                    return true;
                }
            }
        } else if (event->modifiers() == Qt::NoModifier) {
            if (kevent->key() == Qt::Key_Escape) {
                if (m_multitaskview) {
                    m_multitaskview->exit(nullptr);
                    m_currentMode = CurrentMode::Normal;
                }
            }
        } else if (event->modifiers() == Qt::AltModifier) {
            if (kevent->key() == Qt::Key_F4 && m_activatedSurface) { // close window : Alt + F4
                m_activatedSurface->requestClose();
                return true;
            }
            if (kevent->key() == Qt::Key_Space && m_activatedSurface) {
                Q_EMIT m_activatedSurface->requestShowWindowMenu({ 0, 0 });
                return true;
            }
        }
    }

    if (event->type() == QEvent::KeyRelease) {
        if (m_taskSwitch && m_taskSwitch->property("switchOn").toBool()) {
            auto kevent = static_cast<QKeyEvent *>(event);

            if (kevent->key() == Qt::Key_Alt || kevent->key() == Qt::Key_Meta) {
                auto filter = Helper::instance()->workspace()->currentFilter();
                filter->setFilterAppId("");
                QMetaObject::invokeMethod(m_taskSwitch, "exit");
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        handleLeftButtonStateChanged(event);
    }

    if (event->type() == QEvent::Wheel) {
        handleWhellValueChanged(event);
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    doGesture(event);

    if (auto surface = m_rootSurfaceContainer->moveResizeSurface()) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            auto ownsOutput = surface->ownsOutput();
            if (!ownsOutput) {
                m_rootSurfaceContainer->endMoveResize();
                return false;
            }

            auto lastPosition =
                m_fakelastPressedPosition.value_or(cursor->lastPressedOrTouchDownPosition());
            auto increment_pos = ev->globalPosition() - lastPosition;
            m_rootSurfaceContainer->doMoveResize(increment_pos);

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_rootSurfaceContainer->endMoveResize();
            m_fakelastPressedPosition.reset();
        }
    }

    // handle shortcut
    if (m_currentMode == CurrentMode::Normal && event->type() == QEvent::KeyRelease) {
        do {
            auto kevent = static_cast<QKeyEvent *>(event);
            // SKIP Meta+Meta
            if (kevent->key() == Qt::Key_Meta && kevent->modifiers() == Qt::NoModifier
                && !m_singleMetaKeyPendingPressed) {
                break;
            }
            bool isFind = false;
            QKeySequence sequence(kevent->modifiers() | kevent->key());
            for (auto *action : m_shortcut->actions(m_currentUserId)) {
                if (action->shortcut() == sequence) {
                    isFind = true;
                    action->activate(QAction::Trigger);
                }
            }
            if (isFind) {
                return true;
            }
        } while (false);
    }

    return false;
}

bool Helper::afterHandleEvent([[maybe_unused]] WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_rootSurfaceContainer->getSurface(watched);
        activateSurface(surface, Qt::MouseFocusReason);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
            activateSurface(nullptr, Qt::OtherFocusReason);
        }
    }

    return false;
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

Output *Helper::createNormalOutput(WOutput *output)
{
    Output *o = Output::create(output, qmlEngine(), this);
    o->outputItem()->stackBefore(m_rootSurfaceContainer);
    m_rootSurfaceContainer->addOutput(o);
    return o;
}

Output *Helper::createCopyOutput(WOutput *output, Output *proxy)
{
    return Output::createCopy(output, proxy, qmlEngine(), this);
}

void Helper::toggleMultitaskview(Multitaskview::ActiveReason reason)
{
    if (!m_multitaskview) {
        toggleOutputMenuBar(false);
        workspace()->setSwitcherEnabled(false);
        m_multitaskview =
            qobject_cast<Multitaskview *>(qmlEngine()->createMultitaskview(rootContainer()));
        connect(m_multitaskview.data(), &Multitaskview::visibleChanged, this, [this] {
            if (!m_multitaskview->isVisible()) {
                m_multitaskview->deleteLater();
                toggleOutputMenuBar(true);
                workspace()->setSwitcherEnabled(true);
            }
        });
        m_currentMode = CurrentMode::Multitaskview;
        m_multitaskview->enter(reason);
    } else {
        if (m_multitaskview->status() == Multitaskview::Exited) {
            m_multitaskview->enter(Multitaskview::ShortcutKey);
            m_currentMode = CurrentMode::Multitaskview;
        } else {
            m_multitaskview->exit(nullptr);
            m_currentMode = CurrentMode::Normal;
        }
    }
}

SurfaceWrapper *Helper::keyboardFocusSurface() const
{
    auto item = m_renderWindow->activeFocusItem();
    if (!item)
        return nullptr;
    auto surface = qobject_cast<WSurfaceItem *>(item->parent());
    if (!surface)
        return nullptr;
    return qobject_cast<SurfaceWrapper *>(surface->parent());
}

void Helper::reuqestKeyboardFocusForSurface(SurfaceWrapper *newActivate, Qt::FocusReason reason)
{
    auto *nowKeyboardFocusSurface = keyboardFocusSurface();
    if (nowKeyboardFocusSurface == newActivate)
        return;

    Q_ASSERT(!newActivate
             || newActivate->shellSurface()->hasCapability(WToplevelSurface::Capability::Focus));

    if (nowKeyboardFocusSurface && nowKeyboardFocusSurface->hasActiveCapability()) {
        if (newActivate) {
            if (nowKeyboardFocusSurface->shellSurface()->keyboardFocusPriority()
                > newActivate->shellSurface()->keyboardFocusPriority())
                return;
        } else {
            if (nowKeyboardFocusSurface->shellSurface()->keyboardFocusPriority() > 0)
                return;
        }
    }

    if (nowKeyboardFocusSurface)
        nowKeyboardFocusSurface->setFocus(false, reason);

    if (newActivate)
        newActivate->setFocus(true, reason);
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
        Q_ASSERT(newActivateSurface->showOnWorkspace(workspace()->current()->id()));
        newActivateSurface->stackToLast();
    }

    if (m_activatedSurface)
        m_activatedSurface->setActivate(false);

    if (newActivateSurface) {
        if (m_showDesktop == WindowManagementV1::DesktopState::Show)
            m_showDesktop = WindowManagementV1::DesktopState::Normal;

        newActivateSurface->setActivate(true);
        workspace()->pushActivedSurface(newActivateSurface);
    }
    m_activatedSurface = newActivateSurface;
    Q_EMIT activatedSurfaceChanged();
}

void Helper::setCursorPosition(const QPointF &position)
{
    m_rootSurfaceContainer->endMoveResize();
    m_seat->setCursorPosition(position);
}

void Helper::handleRequestDrag(WSurface *surface)
{
    m_seat->setAlwaysUpdateHoverTarget(true);

    struct wlr_drag *drag = m_seat->nativeHandle()->drag;
    Q_ASSERT(drag);
    QObject::connect(qw_drag::from(drag), &qw_drag::notify_drop, this, [this]{
        if (m_ddeShellV1)
            m_ddeShellV1->sendDrop(m_seat);
    });

    QObject::connect(qw_drag::from(drag), &qw_drag::before_destroy, this, [this, surface, drag]{
        if (surface)
            surface->safeDeleteLater();

        drag->data = NULL;
        m_seat->setAlwaysUpdateHoverTarget(false);
    });

    if (m_ddeShellV1)
        m_ddeShellV1->sendStartDrag(m_seat);
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    output->safeConnect(&qw_output::notify_request_state,
                        this,
                        [this](wlr_output_event_request_state *newState) {
                            if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
                                auto output = qobject_cast<qw_output *>(sender());

                                if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
                                    output->set_custom_mode(newState->state->custom_mode.width,
                                                            newState->state->custom_mode.height,
                                                            newState->state->custom_mode.refresh);
                                } else {
                                    output->set_mode(newState->state->mode);
                                }

                                output->commit();
                            }
                        });
}

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frame. Because QWOutput::frame signal
    // maybe emit before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this output on render.
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
    qobject_cast<qw_multi_backend *>(m_backend->handle())
        ->for_each_backend(
            [](wlr_backend *backend, void *) {
                if (auto x11 = qw_x11_backend::from(backend)) {
                    qw_output::from(x11->output_create());
                } else if (auto wayland = qw_wayland_backend::from(backend)) {
                    qw_output::from(wayland->output_create());
                }
            },
            nullptr);
}

void Helper::setOutputMode(OutputMode mode)
{
    if (m_outputList.length() < 2 || m_mode == mode)
        return;
    m_mode = mode;
    Q_EMIT outputModeChanged();
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i) == m_rootSurfaceContainer->primaryOutput())
            continue;
        Output *o;
        if (mode == OutputMode::Copy) {
            o = createCopyOutput(m_outputList.at(i)->output(),
                                 m_rootSurfaceContainer->primaryOutput());
            m_rootSurfaceContainer->removeOutput(m_outputList.at(i));
        } else if (mode == OutputMode::Extension) {
            o = createNormalOutput(m_outputList.at(i)->output());
            enableOutput(o->output());
        }
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
}

void Helper::setOutputProxy(Output *output) { }

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

void Helper::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

WXWayland *Helper::createXWayland()
{
    return m_shellHandler->createXWayland(m_server, m_seat, m_compositor, false);
}

void Helper::removeXWayland(WXWayland *xwayland)
{
    m_shellHandler->removeXWayland(xwayland);
}

WSocket *Helper::defaultWaylandSocket() const
{
    return m_socket;
}

WXWayland *Helper::defaultXWaylandSocket() const
{
    return m_defaultXWayland;
}

PersonalizationV1 *Helper::personalization() const
{
    return m_personalization;
}

void Helper::toggleOutputMenuBar(bool show)
{
#ifdef QT_DEBUG
    for (const auto &output : rootContainer()->outputs()) {
        output->outputMenuBar()->setVisible(show);
    }
#endif
}

QString Helper::cursorTheme() const
{
    return TreelandConfig::ref().cursorThemeName();
}

QSize Helper::cursorSize() const
{
    return TreelandConfig::ref().cursorSize();
}

WindowManagementV1::DesktopState Helper::showDesktopState() const
{
    return m_showDesktop;
}

bool Helper::isLaunchpad(WLayerSurface *surface) const
{
    if (!surface) {
        return false;
    }

    auto scope = QString(surface->handle()->handle()->scope);

    return scope == "dde-shell/launchpad";
}

WSeat *Helper::seat() const
{
    return m_seat;
}

void Helper::handleLeftButtonStateChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_seat);
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_ddeShellV1->sendActiveIn(TREELAND_DDE_ACTIVE_V1_REASON_MOUSE, m_seat);
        } else {
            m_ddeShellV1->sendActiveOut(TREELAND_DDE_ACTIVE_V1_REASON_MOUSE, m_seat);
        }
    }
}

void Helper::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_seat);
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        m_ddeShellV1->sendActiveOut(TREELAND_DDE_ACTIVE_V1_REASON_WHEEL, m_seat);
    }
    if (delta.x() + delta.y() > 0) {
        m_ddeShellV1->sendActiveIn(TREELAND_DDE_ACTIVE_V1_REASON_WHEEL, m_seat);
    }
}
