// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscopedvalue.h>
#include "helper.h"
#include "pointerconstraintsmanager.h"
#include "seatmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QPoint>
#include <qnamespace.h>
#ifdef EXT_SESSION_LOCK_V1
#include "wsessionlock.h"
#include "wsessionlockmanager.h"
#include "core/lockscreen.h"
#endif
#ifndef DISABLE_DDM
#include "core/lockscreen.h"
#endif
#include "common/treelandlogging.h"
#include "common/shellaction.h"
#include "core/layersurfacecontainer.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/dconfigmanager.h"
#include "core/treeland.h"
#include "core/windowpicker.h"
#include "greeter/greeterproxy.h"
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "input/inputdevice.h"
#include "inputmanager.h"
#include "interfaces/multitaskviewinterface.h"
#include "modules/capture/capture.h"
#include "modules/dde-shell/ddeshellattached.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev2.h"
#include "modules/ddm/ddminterfacev1.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "modules/keyboard-shortcuts-inhibit/keyboardshortcutsinhibitmanager.h"
#include "modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h"
#include "modules/active-notify/activenotifymanagerinterfacev1.h"
#include "modules/region-watch/regionwatchmanagerinterfacev1.h"
#include "modules/output-manager/outputmanagement.h"
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "modules/appearance/appearanceinterfacev1.h"
#include "modules/appearance/appearancemanagerinterfacev1.h"
#include "modules/compositor-action/compositoractioninterfacev1.h"
#include "modules/decoration/decorationmanagerinterfacev1.h"
#include "modules/resource/treelandremotesource.h"
#include "modules/screensaver/screensaverinterfacev2.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/shortcut/shortcutrunner.h"
#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"
#include "output/output.h"
#include "output/outputmanager.h"
#include "seatuserconfig.hpp"
#include "session/session.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "utils/cmdline.h"
#include "utils/fpsdisplaymanager.h"
#include "wallpaper/wallpapermanager.h"
#include "wallpapershellinterfacev1.h"
#include "workspace/workspace.h"

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <WBackend>
#include <WForeignToplevel>
#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <WXdgOutput>
#include <wayland-util.h>
#include <wcursorshapemanagerv1.h>
#include <wextimagecapturesourcev1impl.h>
#include <wlayersurface.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <wpointerconstraintsv1.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wrelativepointermanagerv1.h>
#include <wremotesubsurfacemanagerv1.h>
#include <wrenderhelper.h>
#include <wscoplistener.h>
#include <wseat.h>
#include <wsecuritycontextmanager.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgtopleveltagmanager.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <DGuiApplicationHelper>

#include <QAction>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlContext>
#include <QQuickWindow>
#include <QThreadPool>

#include <algorithm>
#include <memory>
#include <pwd.h>
#include <unistd.h>
#include <utility>

#define EXT_DATA_CONTROL_MANAGER_V1_VERSION 1
#define WLR_FRACTIONAL_SCALE_V1_VERSION 1
#define DEFAULT_SEAT_NAME "seat0"

static bool userConfigInitializationFinished(TreelandUserConfig *config)
{
    return config && (config->isInitializeSucceeded() || config->isInitializeFailed());
}

static bool seatConfigInitializationFinished(SeatUserDConfig *config)
{
    return config && (config->isInitializeSucceeded() || config->isInitializeFailed());
}

Helper *Helper::m_instance = nullptr;

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , WObject()
    , m_sessionManager(new SessionManager(this))
    , m_wallpaperManager(new WallpaperManager(this))
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_inputManager(new InputManager(this))
{
    m_isDDMDisplay = qEnvironmentVariableIsSet("DDM_DISPLAY_MANAGER");
    Q_ASSERT(!m_instance);
    m_instance = this;

    if (auto *configManager = DConfigManager::instance()) {
        m_config = configManager->initialUserConfig();
        m_globalConfig = configManager->globalConfig();
    }

    Q_ASSERT(m_config);
    Q_ASSERT(m_globalConfig);

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer, m_server);
    connect(m_shellHandler->workspace(),
            &Workspace::workspaceAdded,
            m_wallpaperManager,
            &WallpaperManager::syncAddWorkspace);
    tryInitRemoteSource();
#ifndef ALWAYS_ENABLE_TREELAND_DEBUG
    // Release builds: react to runtime changes of the remoteDebug DConfig key,
    // creating or destroying the remote source on the fly instead of only at
    // startup.
    connect(m_globalConfig,
            &TreelandConfig::remoteDebugChanged,
            this,
            &Helper::tryInitRemoteSource);
#endif

    m_outputManager = new OutputManager(m_rootSurfaceContainer,
                                        m_globalConfig,
                                        m_wallpaperManager,
                                        m_shellHandler->workspace(),
                                        m_renderWindow,
                                        this);
    connect(m_outputManager, &OutputManager::modeChanged, this, &Helper::outputModeChanged);

#ifdef EXT_SESSION_LOCK_V1
    m_lockScreenGraceTimer = new QTimer(this);
    m_lockScreenGraceTimer->setInterval(300);
    m_lockScreenGraceTimer->setSingleShot(true);
#endif

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow,
            &QQuickWindow::activeFocusItemChanged,
            this,
            &Helper::onRenderWindowActiveFocusItemChanged);

    // Connect to systemd-logind's PrepareForSleep signal for hibernate blackout
    // Also connect to SessionNew signal for logging purposes
    QDBusConnection::systemBus().connect(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "SessionNew",
        this,
        SLOT(onSessionNew(QString,QDBusObjectPath))
    );
}

Helper::~Helper()
{
    Q_ASSERT(m_instance == this);
    teardown();

    m_instance = nullptr;
    if (m_renderWindow) {
        m_renderWindow->disconnect();
    }

    if (m_backend) {
        m_backend->disconnect();
    }

    m_currentEventSeat = nullptr;
    // destroy before m_rootSurfaceContainer
    delete m_shellHandler;
    if (m_rootSurfaceContainer) {
        for (auto s : std::as_const(m_rootSurfaceContainer->surfaces())) {
            if (auto c = s->container())
                c->removeSurface(s);
        }
        delete m_rootSurfaceContainer;
    }
}

Helper *Helper::instance()
{
    return m_instance;
}

TreelandUserConfig *Helper::config()
{
    return m_config;
}

TreelandConfig *Helper::globalConfig()
{
    return m_globalConfig;
}

void Helper::syncPaletteTypeWithWindowColorScheme(int32_t colorScheme)
{
    auto *guiHelper = DTK_GUI_NAMESPACE::DGuiApplicationHelper::instance();
    if (!guiHelper) {
        qCCritical(lcTlConfig) << "DGuiApplicationHelper instance not available, cannot sync "
                                      "palette type with window color scheme.";
        return;
    }

    qCDebug(lcTlConfig) << "Syncing palette type with window color scheme:" << colorScheme;

    switch (colorScheme) {
    case 1:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::DarkType);
        break;
    case 0:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    default:
        qCWarning(lcTlConfig) << "Unknown windowColorScheme:" << colorScheme
                                  << ", fallback to light.";
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    }
}

void Helper::tryInitRemoteSource()
{
#ifdef ALWAYS_ENABLE_TREELAND_DEBUG
    // Remote debug is on by default in Debug builds so treeland-debug works out
    // of the box. The source has zero cost until a client connects, so enabling
    // it unconditionally here is safe even with no debug client attached.
    if (m_treelandRemoteSource)
        return;
    m_treelandRemoteSource = new TreelandRemoteSource(this);
    return;
#else
    // Release builds: follow the remoteDebug DConfig key (default false) and
    // react to its runtime changes -- toggling the key creates or destroys the
    // remote source without restarting the compositor.
    if (m_treelandRemoteSource) {
        if (!m_globalConfig->remoteDebug()) {
            delete m_treelandRemoteSource;
            m_treelandRemoteSource = nullptr;
        }
    } else if (m_globalConfig->remoteDebug()) {
        m_treelandRemoteSource = new TreelandRemoteSource(this);
    }
#endif
}

void Helper::setWorkspaceVisible(bool visible)
{
    for (auto *surface : std::as_const(m_rootSurfaceContainer->surfaces())) {
        if (surface->type() == SurfaceWrapper::Type::Layer) {
            surface->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        }
    }

    if (m_noAnimation) {
        m_shellHandler->workspace()->setOpacity(visible ? 1.0 : 0.0);
        m_shellHandler->workspace()->setScale(visible ? 1.0 : 1.4);
        return;
    }

    if (visible) {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.0);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(1.0);
        m_workspaceOpacityAnimation->start();
    } else {
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

QmlEngine *Helper::qmlEngine() const
{
    return qobject_cast<QmlEngine *>(::qmlEngine(this));
}

WOutputRenderWindow *Helper::window() const
{
    return m_renderWindow;
}

SessionManager *Helper::sessionManager() const
{
    return m_sessionManager;
}

ShellHandler *Helper::shellHandler() const
{
    return m_shellHandler;
}

Workspace *Helper::workspace() const
{
    return m_shellHandler->workspace();
}

void Helper::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_rootSurfaceContainer->getSurface(surface);
    if (!s)
        return;
    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void Helper::onNewIdleInhibitor(wlr_idle_inhibitor_v1 *wlr_inhibitor)
{
    if (!wlr_inhibitor->surface) {
        qCInfo(lcTlCore) << "Ignoring idle inhibitor with null surface";
        return;
    }

    auto wsurface = WSurface::fromHandle(wlr_inhibitor->surface);
    if (!wsurface) {
        qCWarning(lcTlCore) << "No WSurface found for idle inhibitor surface"
                                << wlr_inhibitor->surface;
        return;
    }

    m_idleInhibitors.append(wlr_inhibitor);

    IdleInhibitorEntry entry;
    entry.inhibitor = wlr_inhibitor;
    // Single destroy signal: keep a WScopedListener on the entry itself
    // (equivalent to the old QObject context / QMetaObject::Connection).
    // Erasing the entry disconnects the listener via RAII.
    entry.destroyListener.init(&wlr_inhibitor->events.destroy, [this, wlr_inhibitor] (void *) {
        m_idleInhibitors.removeOne(wlr_inhibitor);
        auto it = std::find_if(idleInhibitorEntries.begin(), idleInhibitorEntries.end(),
                               [wlr_inhibitor](const auto &e) {
            return e.inhibitor == wlr_inhibitor;
        });
        if (it != idleInhibitorEntries.end()) {
            auto connections = std::move(it->connections);
            idleInhibitorEntries.erase(it);
            for (const auto &connection : connections)
                QObject::disconnect(connection);
        }
        updateIdleInhibitor();
    });

    QList<QMetaObject::Connection> connections;
    connections << connect(wsurface, &WSurface::mappedChanged, this, &Helper::updateIdleInhibitor);

    auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
    if (toplevel) {
        connections << connect(toplevel, &WXdgToplevelSurface::minimizeChanged, this, &Helper::updateIdleInhibitor);
    }
    entry.connections = std::move(connections);
    idleInhibitorEntries.push_back(std::move(entry));

    updateIdleInhibitor();
}

void Helper::updateIdleInhibitor()
{
    if (m_screensaverInterfaceV2->isInhibited()) {
        wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, true);
        return;
    }
    for (auto *inhibitor : std::as_const(m_idleInhibitors)) {
        auto wsurface = WSurface::fromHandle(inhibitor->surface);
        if (!wsurface)
            continue;
        bool visible = wsurface->mapped();
        auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
        if (toplevel)
            visible &= !toplevel->isMinimized();

        if (visible) {
            wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, true);
            return;
        }
    }
    wlr_idle_notifier_v1_set_inhibited(m_idleNotifier, false);
}

void Helper::onShowDesktop()
{
    ShowDesktopInterfaceV1::State s = m_showDesktopInterfaceV1->desktopState();
    if (m_showDesktop == s
        || (s != ShowDesktopInterfaceV1::State::Normal
            && s != ShowDesktopInterfaceV1::State::Show))
        return;

    m_showDesktop = s;
    Q_EMIT showDesktopStateChanged();
    const auto &surfaces = m_outputManager->workspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface->isMinimized()) {
            continue;
        }
        if (s == ShowDesktopInterfaceV1::State::Normal) {
            surface->startShowDesktopAnimation(true);
        } else if (s == ShowDesktopInterfaceV1::State::Show) {
            surface->startShowDesktopAnimation(false);
        }
    }

    if (s == ShowDesktopInterfaceV1::State::Show) {
        // Find the desktop background surface first
        SurfaceWrapper *desktopSurface = nullptr;
        const auto &backgroundSurfaces = m_shellHandler->m_backgroundContainer->surfaces();
        for (SurfaceWrapper *w : backgroundSurfaces) {
            auto *layer = qobject_cast<WLayerSurface *>(w->shellSurface());
            if (layer && layer->scope() == QStringLiteral("dde-shell/desktop")) {
                desktopSurface = w;
                break;
            }
        }

        const auto &seats = m_seatManager->seats();
        for (auto *seat : seats) {
            auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);
            // Seat may be in transition (hotplug); requestKeyboardFocus asserts
            // on a missing seat container.
            if (!seatContainer)
                continue;

            // Dismiss any popup keyboard grab: the grab would redirect keyboard
            // focus back to the popup and defeat the desktop-surface transfer.
            seatContainer->dismissPopups();
            // Move keyboard focus to the desktop surface (or drop it when the
            // desktop surface is unavailable) so layer-shell windows that close
            // on focus loss exit.
            requestKeyboardFocus(desktopSurface, Qt::OtherFocusReason, seat);
        }
    } else if (s == ShowDesktopInterfaceV1::State::Normal) {
        // m_showDesktop already set to s above; the protocol state is already Normal.
        restoreShowDesktopFocus();
    }
}

void Helper::restoreShowDesktopFocus()
{
    const auto &seats = m_seatManager->seats();
    for (auto *seat : seats) {
        if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat))
            seatContainer->restoreShowDesktopFocus();
    }
}

void Helper::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    connect(m_sessionManager->activeSession().lock().get(),
            &Session::aboutToBeDestroyed,
            wrapper,
            &SurfaceWrapper::closeSurface);

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), m_personalizationInterfaceV1, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        // New SSD personalization protocol (treeland-decoration-unstable-v1).
        // The frozen Personalization above handles the deprecated
        // treeland-personalization-manager-v1 window context; per-window
        // customization for the new protocol is applied via Decoration.
        auto *decoration =
            new Decoration(wrapper->shellSurface(), m_decorationInterfaceV1, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                decoration, &Decoration::deleteLater);

        // Single arbitration point for SurfaceWrapper::noTitleBar. Two
        // independent sources feed this: the frozen personalization protocol
        // (Personalization::noTitlebar) and the new per-window decoration
        // protocol (Decoration::titlebarHidden). Both write into the same
        // wrapper property, so the final value is decided here.
        auto updateNoTitlebar = [this, attached, decoration] {
            auto wrapper = attached->surfaceWrapper();
            const bool decorationHides = decoration->hasTitlebarOverride()
                && decoration->titlebarHidden();

            if (attached->noTitlebar() || decorationHides) {
                wrapper->setNoTitleBar(true);
                if (!wrapper->isLaunchpad()) {
                    wrapper->setNoDecoration(false);
                }
            } else {
                wrapper->setNoTitleBar(false);
                wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
            }
        };

        if (isXdgToplevel) {
            connect(
                m_xdgDecorationManager,
                &WXdgDecorationManager::surfaceModeChanged,
                attached,
                [attached, updateNoTitlebar](
                    WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                    [[maybe_unused]] Waylib::Server::WXdgDecorationManager::DecorationMode mode) {
                    if (surface == attached->surfaceWrapper()->surface()) {
                        updateNoTitlebar();
                    }
                });
        }

        connect(attached, &Personalization::windowStateChanged, this, updateNoTitlebar);
        // Re-arbitrate the titlebar whenever the decoration protocol changes
        // its override, so it can't be clobbered by (nor clobber) the
        // personalization path above.
        connect(decoration, &Decoration::titlebarOverrideChanged, this, updateNoTitlebar);
        updateNoTitlebar();

        auto updateBlur = [attached] {
            attached->surfaceWrapper()->setBlur(attached->backgroundType() == Personalization::BackgroundType::Blur);
        };
        connect(attached, &Personalization::backgroundTypeChanged, this, updateBlur);

        // NOTE: corner radius is written by BOTH the deprecated personalization
        // protocol (here) and the new decoration protocol (Decoration::applyContext).
        // The two are not arbitrated; if a client uses both, the last signal to
        // arrive wins. This is intentional: clients are expected to use only one
        // of the two protocols. When the deprecated personalization protocol is
        // removed, drop this block and its setRadius() call — the decoration
        // protocol already drives SurfaceWrapper::setRadius() on its own.
        auto updateCornerRadius = [attached] {
            attached->surfaceWrapper()->setRadius(attached->cornerRadius());
        };
        connect(attached, &Personalization::cornerRadiusChanged, this, updateCornerRadius);
        updateCornerRadius();
        updateBlur();
        if (wrapper->isLaunchpad())
            wrapper->setCoverEnabled(true);
    }

    if (isXwayland) {
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        auto updateDecorationTitleBar = [wrapper, xwaylandSurface, sessionManager = m_sessionManager]() {
            auto *xwayland = xwaylandSurface->xwayland();
            const auto session = xwayland
                ? sessionManager->sessionForXWayland(xwayland)
                : std::shared_ptr<Session>();
            xcb_atom_t noTitlebarAtom = XCB_ATOM_NONE;
            if (session)
                noTitlebarAtom = session->noTitlebarAtom();
            const auto flags = xwaylandSurface->effectiveDecorationsFlags(noTitlebarAtom);
            wrapper->setNoTitleBar(flags & WXWaylandSurface::DecorationsNoTitle);
            wrapper->setNoDecoration(flags & WXWaylandSurface::DecorationsNoBorder);
        };
        // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
        // but WXWaylandSurface will not, so must connect to `wrapper`
        QObject::connect(xwaylandSurface, &WXWaylandSurface::bypassManagerChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        QObject::connect(xwaylandSurface, &WXWaylandSurface::decorationsFlagsChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        updateDecorationTitleBar();

        wrapper->setHideByWorkspace(!surfaceBelongsToCurrentSession(wrapper));
    }

    if (!isLayer) {
        [[maybe_unused]] auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);

        if (m_regionWatchManagerInterfaceV1)
            m_regionWatchManagerInterfaceV1->addSurface(wrapper);
    }

#ifndef DISABLE_DDM
    if (isLayer) {
        connect(this, &Helper::currentModeChanged, wrapper, [this, wrapper] {
            wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        });
        wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
    }
#endif

    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->addSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
        } else {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
        }
    });
}

void Helper::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    if (m_regionWatchManagerInterfaceV1)
        m_regionWatchManagerInterfaceV1->removeSurface(wrapper);

    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
    }
    // Ensure the wrapper is removed from active history early to avoid cascading on half-invalid entries
    if (wrapper && wrapper->workspaceId() != -1) {
        auto ws = workspace();
        if (ws) {
            Q_ASSERT(ws == wrapper->container());
            ws->removeActivedSurface(wrapper);
        }
    }
}

bool Helper::surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper)
{
    if (wrapper->type() == SurfaceWrapper::Type::SplashScreen) {
        // TODO(rewine): Determine which user the splash screen belongs to by invoking the client of the prelaunch-splash protocol.
        // Currently, treeland does not support logging in with multiple users at the same time
        // so it is temporarily assumed that the splash screen must belong to the current user.
        return true;
    }
    WClient *client = wrapper->surface()->waylandClient();
    WSocket *socket = client ? client->socket()->rootSocket() : nullptr;
    return socket && socket->isEnabled();
}

void Helper::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
    if (m_currentMode == CurrentMode::WindowSwitch) {
        setCurrentMode(CurrentMode::Normal);
    }
}

void Helper::updateCurrentUser()
{
    const QString userName = m_userModel->currentUserName();
    auto *configManager = DConfigManager::instance();
    auto *userConfig = configManager ? configManager->userConfig(userName) : m_config;
    WSeat *defaultSeat = m_seatManager->getSeat(DEFAULT_SEAT_NAME);
    const QString seatName = defaultSeat ? defaultSeat->name() : QStringLiteral(DEFAULT_SEAT_NAME);
    auto *seatConfig = configManager ? configManager->userSeatConfig(userName, seatName) : nullptr;
    if (!userConfig) {
        qCWarning(lcTlConfig) << "Cannot switch to user" << userName
                              << "because its DConfig object is unavailable";
        return;
    }

    if (userConfig == m_config && (!seatConfig || seatConfig == m_pendingSeatConfig)) {
        applyCurrentUserConfig(userName, userConfig, seatConfig);
        return;
    }

    if (m_pendingUserConfig) {
        QObject::disconnect(static_cast<const QObject *>(m_pendingUserConfig),
                            nullptr,
                            this,
                            nullptr);
    }
    if (m_pendingSeatConfig) {
        QObject::disconnect(static_cast<const QObject *>(m_pendingSeatConfig),
                            nullptr,
                            this,
                            nullptr);
    }

    m_pendingUserName = userName;
    m_pendingUserConfig = userConfig;
    m_pendingSeatConfig = seatConfig;

    if (!userConfigInitializationFinished(userConfig)) {
        connect(userConfig,
                &TreelandUserConfig::configInitializeSucceed,
                this,
                &Helper::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
        connect(userConfig,
                &TreelandUserConfig::configInitializeFailed,
                this,
                &Helper::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
    }
    if (seatConfig && !seatConfigInitializationFinished(seatConfig)) {
        connect(seatConfig,
                &SeatUserDConfig::configInitializeSucceed,
                this,
                &Helper::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
        connect(seatConfig,
                &SeatUserDConfig::configInitializeFailed,
                this,
                &Helper::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
    }

    qCInfo(lcTlConfig) << "Waiting for user DConfig initialization before switching to"
                       << userName;
    onPendingUserConfigInitialized();
}

void Helper::onPendingUserConfigInitialized()
{
    if (sender() && sender() != m_pendingUserConfig && sender() != m_pendingSeatConfig) {
        return;
    }

    if (!userConfigInitializationFinished(m_pendingUserConfig)
        || (m_pendingSeatConfig && !seatConfigInitializationFinished(m_pendingSeatConfig))) {
        return;
    }

    const QString userName = m_pendingUserName;
    auto *userConfig = m_pendingUserConfig;
    auto *seatConfig = m_pendingSeatConfig;
    m_pendingUserName.clear();
    m_pendingUserConfig = nullptr;
    m_pendingSeatConfig = nullptr;

    if (m_userModel->currentUserName() != userName) {
        qCInfo(lcTlConfig) << "Discarding stale user DConfig initialization for" << userName;
        return;
    }

    qCInfo(lcTlConfig) << "User DConfig initialization finished for" << userName
                       << "; applying user configuration";
    if (userConfig->isInitializeFailed()
        || (seatConfig && seatConfig->isInitializeFailed())) {
        qCWarning(lcTlConfig) << "Using generated defaults for user DConfig" << userName;
    }
    applyCurrentUserConfig(userName, userConfig, seatConfig);
}

void Helper::applyCurrentUserConfig(const QString &userName,
                                    TreelandUserConfig *config,
                                    SeatUserDConfig *seatConfig)
{
    if (!config || m_userModel->currentUserName() != userName) {
        return;
    }

    const bool configPointerChanged = config != m_config;
    if (configPointerChanged) {
        if (m_config) {
            disconnect(m_config,
                       &TreelandUserConfig::cursorThemeNameChanged,
                       m_sessionManager,
                       &SessionManager::syncActiveSessionCursorSettings);
            disconnect(m_config,
                       &TreelandUserConfig::cursorSizeChanged,
                       m_sessionManager,
                       &SessionManager::syncActiveSessionCursorSettings);
        }
        m_config = config;
        Q_EMIT configChanged();
    }

    connect(m_config,
            &TreelandUserConfig::cursorThemeNameChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionCursorSettings,
            Qt::UniqueConnection);
    connect(m_config,
            &TreelandUserConfig::cursorSizeChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionCursorSettings,
            Qt::UniqueConnection);

    auto user = m_userModel->currentUser();
    m_personalizationInterfaceV1->setUserId(user ? user->UID() : getuid());
    if (userName == "dde") {
        return;
    }

    if (seatConfig) {
        m_inputManager->setupSeatUserConfig(userName);
    }
    m_sessionManager->syncActiveSessionCursorSettings();
    syncPaletteTypeWithWindowColorScheme(m_config->windowColorScheme());
    m_wallpaperManager->updateWallpaperConfig();
    tryInitRemoteSource();
    // TODO(YaoBing Xiao): Isolate workspaces for different users to prevent them from sharing the same one.
    m_shellHandler->workspace()->reloadFromConfig();
}

void Helper::init(Treeland::Treeland *treeland)
{
    m_treeland = treeland;
    connect(m_sessionManager, &SessionManager::sessionChanged, treeland, &Treeland::Treeland::SessionChanged);
    connect(m_sessionManager,
            &SessionManager::xwaylandAuthChanged,
            treeland,
            &Treeland::Treeland::XWaylandAuthChanged);

    auto engine = qmlEngine();
    m_greeterProxy = engine->singletonInstance<GreeterProxy *>("Treeland", "GreeterProxy");
    m_userModel = engine->singletonInstance<UserModel *>("Treeland", "UserModel");
    m_sessionModel = engine->singletonInstance<SessionModel *>("Treeland", "SessionModel");

    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);
    m_rootSurfaceContainer->init(m_server);

    m_backend = m_server->attach<WBackend>();
    m_seatManager = new SeatManager(m_server, this);

    m_ddmInterfaceV1 = m_server->attach<DDMInterfaceV1>();

    auto *outputManager = m_server->attach<WOutputManagerV1>();
    m_outputManager->setBackend(m_backend);
    m_outputManager->setOutputManagementProtocol(outputManager);

    m_ddeShellV1 = m_server->attach<DDEShellManagerInterfaceV1>();

    m_regionWatchManagerInterfaceV1 = m_server->attach<TreelandRegionWatchManagerInterfaceV1>();

    connect(m_ddeShellV1, &DDEShellManagerInterfaceV1::toggleMultitaskview, this, [this] {
        if (m_multitaskView) {
            m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        }
    });
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::PickerCreated,
            this,
            &Helper::handleWindowPicker);
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::lockScreenCreated,
            this,
            &Helper::handleLockScreen);

    m_ddeShellV2 = m_server->attach<DDEShellManagerInterfaceV2>();

    m_compositorActionInterfaceV1 = m_server->attach<CompositorActionInterfaceV1>();
    connect(m_compositorActionInterfaceV1,
            &CompositorActionInterfaceV1::triggered,
            this,
            &Helper::handleCompositorAction);
    m_shellHandler->createComponent(engine, m_renderWindow->contentItem());

    m_foreignToplevel = m_server->attach<WForeignToplevel>();
    connect(m_foreignToplevel, &WForeignToplevel::requestActivate, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(surface))
                    forceActivateSurface(wrapper, Qt::OtherFocusReason);
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestMaximize, this,
            [this](WToplevelSurface *surface, bool maximized) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(surface)) {
                    if (maximized)
                        wrapper->maximize();
                    else
                        wrapper->unmaximize();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestMinimize, this,
            [this](WToplevelSurface *surface, bool minimized) {
                auto *wrapper = m_rootSurfaceContainer->getSurface(surface);
                if (!wrapper)
                    return;
                if (showDesktopState() == ShowDesktopInterfaceV1::State::Show) {
                    forceActivateSurface(wrapper);
                } else if (minimized) {
                    wrapper->minimize();
                } else {
                    wrapper->restoreFromMinimized();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestFullscreen, this,
            [this](WToplevelSurface *surface, bool fullscreen) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(surface)) {
                    if (fullscreen)
                        wrapper->enterFullscreen();
                    else
                        wrapper->leaveFullscreen();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestClose, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(surface))
                    wrapper->close();
            });
    m_extForeignToplevelListV1 = m_server->attach<WExtForeignToplevelListV1>();
    m_relativePointerManager = m_server->attach<WRelativePointerManagerV1>();
    auto connectSeat = [this](WSeat *seat) {
        connect(seat, &WSeat::relativePointerMotion,
                this, [this, seat](uint32_t ts, QPointF d, QPointF u) {
                    m_relativePointerManager->sendRelativeMotion(seat, ts, d, u);
                });
    };
    connect(m_seatManager, &SeatManager::seatAdded, this, connectSeat);

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

    auto *outputManagerV1 = m_server->attach<OutputManagerV1>();
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            outputManagerV1,
            &OutputManagerV1::onPrimaryOutputChanged);
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionXWaylandPrimaryOutput);
    m_wallpaperColorV1 = m_server->attach<WallpaperColorInterfaceV1>();
    m_showDesktopInterfaceV1 = m_server->attach<ShowDesktopInterfaceV1>();
    m_xWindowControlInterfaceV1 = m_server->attach<XWindowControlInterfaceV1>();
    auto *virtualOutputInterface = m_server->attach<VirtualOutputManagerInterfaceV1>();
    m_outputManager->setVirtualOutputInterface(virtualOutputInterface);

    auto captureManagerV1 = m_server->attach<CaptureManagerV1>();
    captureManagerV1->setOutputRenderWindow(m_renderWindow);

    connect(
        captureManagerV1,
        &CaptureManagerV1::contextInSelectionChanged,
        this,
        [this, captureManagerV1] {
            if (captureManagerV1->contextInSelection()) {
                m_captureSelector = qobject_cast<CaptureSourceSelector *>(
                    qmlEngine()->createCaptureSelector(m_rootSurfaceContainer, captureManagerV1));
            } else if (m_captureSelector) {
                m_captureSelector->deleteLater();
            }
        });
    m_personalizationInterfaceV1 = m_server->attach<PersonalizationManagerInterfaceV1>();

    // New protocols (treeland-protocols 0.6.0)
    m_appearanceInterfaceV1 = m_server->attach<AppearanceInterfaceV1>();
    m_appearanceManagerInterfaceV1 = m_server->attach<AppearanceManagerInterfaceV1>();
    m_decorationInterfaceV1 = m_server->attach<DecorationManagerInterfaceV1>();

    connect(m_userModel, &UserModel::currentUserNameChanged, this, &Helper::updateCurrentUser);

    updateCurrentUser();

    connect(m_showDesktopInterfaceV1,
            &ShowDesktopInterfaceV1::desktopStateChanged,
            this,
            &Helper::onShowDesktop);

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

    m_server->attach<WSecurityContextManager>();

    m_server->start();

    // Initialize seats from configuration
    m_primarySeat =
        m_seatManager->initializeFromConfig(QStringLiteral(TREELAND_SYSCONFDIR "/seats.json"), m_server);
    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "Failed to initialize seats!";
        return;
    }


    // Setup all seats (cursor, keyboard focus, event filter)
    m_seatManager->setupAllSeats(m_renderWindow,
                                 m_rootSurfaceContainer->outputLayout(),
                                 this,
                                 m_rootSurfaceContainer->cursor());

    // Connect device signals and handle device lifecycle
    m_seatManager->connectBackendSignals(m_backend);
    connect(m_seatManager, &SeatManager::deviceAdded, this, [this](WInputDevice *device) {
        m_seatManager->assignDevice(device,
                                    m_renderWindow,
                                    m_rootSurfaceContainer->outputLayout(),
                                    m_primarySeat);
    });

    // Setup drag request handling for all seats
    const auto seats = m_seatManager->seats();
    for (auto *seat : seats) {
        disconnect(seat, &WSeat::requestDrag, this, nullptr);
        connect(seat, &WSeat::requestDrag, this, [this, seat](WSurface *surface) {
            handleRequestDragForSeat(seat, surface);
        });
    }

    // Assign existing devices
    m_seatManager->assignExistingDevices(m_backend);

    if (m_rootSurfaceContainer) {
        m_rootSurfaceContainer->setupSeatManagement();
    }

    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "No seat available after initialization, cannot continue";
        return;
    }
    m_shellHandler->init(m_server, m_primarySeat);

    connect(m_shellHandler->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            m_wallpaperManager,
            &WallpaperManager::handleWallpaperSurfaceAdded);

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qCFatal(lcTlCore) << "Failed to create renderer";
    }

    m_allocator = wlr_allocator_autocreate(m_backend->handle(), m_renderer);
    if (!m_allocator) {
        qCFatal(lcTlCore) << "Failed to create allocator";
    }
    if (!wlr_renderer_init_wl_display(m_renderer, m_server->handle())) {
        qCFatal(lcTlCore) << "Failed to initialize renderer wl_display";
    }
    if (!wlr_drm_create(m_server->handle(), m_renderer)) {
        qCCritical(lcTlCore) << "Failed to create DRM lease manager";
    }

    // free follow display
    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer);
    if (!m_compositor) {
        qCFatal(lcTlCore) << "Failed to create compositor";
    }
    if (!wlr_subcompositor_create(m_server->handle()))
        qCCritical(lcTlCore) << "Failed to create subcompositor";
    if (!wlr_screencopy_manager_v1_create(m_server->handle()))
        qCCritical(lcTlCore) << "Failed to create screencopy manager";
    if (!wlr_ext_image_copy_capture_manager_v1_create(m_server->handle(), 1))
        qCCritical(lcTlCore) << "Failed to create image copy capture manager";
    if (!wlr_ext_output_image_capture_source_manager_v1_create(m_server->handle(), 1))
        qCCritical(lcTlCore) << "Failed to create output image capture source manager";
    m_foreignToplevelImageCaptureManager = wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(m_server->handle(), 1);
    if (m_foreignToplevelImageCaptureManager) {
        listeners()->add(
            &m_foreignToplevelImageCaptureManager->events.new_request, this,
            &Helper::handleNewForeignToplevelCaptureRequest);
    } else {
        qCCritical(lcTlCore) << "Failed to create foreign-toplevel image capture manager; capture disabled";
    }

    if (!wlr_viewporter_create(m_server->handle()))
        qCCritical(lcTlCore) << "Failed to create viewporter";
    if (!wlr_single_pixel_buffer_manager_v1_create(m_server->handle()))
        qCCritical(lcTlCore) << "Failed to create single pixel buffer manager";
    m_renderWindow->init(m_renderer, m_allocator);

    auto *xwaylandOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    m_outputManager->setXdgOutputManagers(
        xdgOutputManager, xwaylandOutputManager, m_sessionManager);
    // User dde does not has a real Logind session, so just pass "0" as id
    m_sessionManager->updateActiveUserSession(QStringLiteral("dde"), QStringLiteral("0"));
    connect(m_userModel, &UserModel::userLoggedIn, m_sessionManager, &SessionManager::updateActiveUserSession);
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &Helper::onSurfaceModeChanged);

    m_xdgDialogManagerV1 = m_server->attach<WXdgDialogManagerV1>();
    connect(m_xdgDialogManagerV1,
            &WXdgDialogManagerV1::surfaceModalChanged,
            this,
            [this](WXdgToplevelSurface *toplevel, bool modal) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(toplevel)) {
                    wrapper->setModal(modal);
                } else {
                    qCWarning(lcTlShell) << "xdg-dialog-v1: no wrapper for toplevel" << toplevel;
                }
            });

    m_xdgToplevelTagManagerV1 = m_server->attach<WXdgToplevelTagManagerV1>();

    auto gammaControlManager = wlr_gamma_control_manager_v1_create(m_server->handle());
    m_outputManager->setGammaControlManager(gammaControlManager);

    m_server->attach<WRemoteSubsurfaceManagerV1>();
    m_server->attach<WCursorShapeManagerV1>();
    m_pointerConstraintsV1 = m_server->attach<WPointerConstraintsV1>();
    m_pointerConstraintsManager = new PointerConstraintsManager(m_pointerConstraintsV1, this);
    wlr_fractional_scale_manager_v1_create(m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    wlr_data_control_manager_v1_create(m_server->handle());
    wlr_ext_data_control_manager_v1_create(m_server->handle(), EXT_DATA_CONTROL_MANAGER_V1_VERSION);
    wlr_alpha_modifier_v1_create(m_server->handle());
    auto *foreignRegistry = wlr_xdg_foreign_registry_create(m_server->handle());
    wlr_xdg_foreign_v2_create(m_server->handle(), foreignRegistry);

    m_idleNotifier = wlr_idle_notifier_v1_create(m_server->handle());

    m_idleInhibitManager = wlr_idle_inhibit_v1_create(m_server->handle());
    listeners()->add(&m_idleInhibitManager->events.new_inhibitor, this, &Helper::onNewIdleInhibitor);

    m_activationManagerV1 = m_server->attach<ActivationManagerInterfaceV1>(
        [this](WSurface *surface, WSeat *seat) -> bool {
            // Determine whether the surface can transfer activation for the same seat
            // that produced the serial in set_serial.
            if (!seat || !seat->isValid()) {
                return false;
            }

            auto wrapper = m_rootSurfaceContainer->getSurface(surface);
            if (!wrapper) {
                return false;
            }

            if (seat->keyboardFocusSurface() == surface) {
                return true;
            }

            if (seat->pointerFocusSurface() == surface) {
                return true;
            }

            if (wrapper->isActivated() && getLastInteractingSeat(wrapper) == seat) {
                return true;
            }

            return false;
        });
    connect(m_activationManagerV1,
            &ActivationManagerInterfaceV1::activateRequested,
            this,
            [this](ActivationManagerInterfaceV1::TokenDisposition disposition, WSurface *wsurface, WSeat *seat) {
                auto wrapper = m_rootSurfaceContainer->getSurface(wsurface);
                if (!wrapper) {
                    qCWarning(lcTlCore) << "Activation request for unknown surface!";
                    return;
                }
                // Don't use hasActiveCapability() here — it also checks UnMinimized,
                // but minimized windows should be allowed to activate (which unminimizes them).
                if (!wsurface->mapped()) {
                    qCWarning(lcTlCore) << "Activation request for unmapped surface!";
                    return;
                }
                if (!wrapper->hasInitializeContainer()) {
                    qCWarning(lcTlCore) << "Activation request for surface without initialized container:"
                                       << "container =" << wrapper->container();
                    return;
                }
                switch (disposition) {
                case ActivationManagerInterfaceV1::TokenDisposition::Active:
                    forceActivateSurface(wrapper, Qt::OtherFocusReason, seat);
                    break;
                case ActivationManagerInterfaceV1::TokenDisposition::Attention:
                    wrapper->setAttention(true);
                    break;
                case ActivationManagerInterfaceV1::TokenDisposition::Invalid:
                    // Use a relaxed policy: fallback to attention if the token is invalid
                    wrapper->setAttention(true);
                    break;
                }
            });

    m_screensaverInterfaceV2 = m_server->attach<ScreensaverInterfaceV2>();

    auto *outputPowerManager = wlr_output_power_manager_v1_create(m_server->handle());
    m_outputManager->setOutputPowerManager(outputPowerManager);
#ifdef EXT_SESSION_LOCK_V1
    m_sessionLockManager = m_server->attach<WSessionLockManager>();
    if (!m_lockScreen) {
        setLockScreenImpl(nullptr);
    }
    connect(m_sessionLockManager,
            &WSessionLockManager::lockCreated,
            this,
            &Helper::onExtSessionLock);
#endif

    m_wallpaperNotifierInterfaceV1 = m_server->attach<TreelandWallpaperNotifierInterfaceV1>();
    if (isDDMDisplay()) {
        m_wallpaperNotifierInterfaceV1->setFilter([this](WClient *client) { return m_sessionManager->isDDEUserClient(client); });
    }
    connect(m_wallpaperNotifierInterfaceV1,
            &TreelandWallpaperNotifierInterfaceV1::bound,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperNotifierBound);

    m_wallpaperManagerInterfaceV1 = m_server->attach<TreelandWallpaperManagerInterfaceV1>();
    connect(m_wallpaperManagerInterfaceV1,
            &TreelandWallpaperManagerInterfaceV1::wallpaperCreated,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperAdded);

    m_shortcutManager = m_server->attach<ShortcutManagerV2>();
    connect(m_treeland,
            &Treeland::Treeland::SessionChanged,
            m_shortcutManager,
            &ShortcutManagerV2::onSessionChanged);
    m_shortcutManager->onSessionChanged();
    auto shortcutControl = m_shortcutManager->controller();
    auto *shortcutRunner = new ShortcutRunner(shortcutControl);
    connect(shortcutControl,
            &ShortcutController::actionTriggered,
            shortcutRunner,
            &ShortcutRunner::onActionTrigger);
    connect(shortcutControl,
            &ShortcutController::actionProgress,
            shortcutRunner,
            &ShortcutRunner::onActionProgress);
    connect(shortcutControl,
            &ShortcutController::actionFinished,
            shortcutRunner,
            &ShortcutRunner::onActionFinish);

    m_inputManagerInterfaceV1 = m_server->attach<TreelandInputManagerInterfaceV1>();
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::mouseSettingsCreated,
            m_inputManager,
            &InputManager::onMouseSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::touchpadSettingsCreated,
            m_inputManager,
            &InputManager::onTouchpadSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::keyboardSettingsCreated,
            m_inputManager,
            &InputManager::onKeyboardSettingsCreated);

    m_keyboardStateNotifyManagerInterfaceV1 = m_server->attach<TreelandKeyboardStateNotifyManagerInterfaceV1>();
    m_activeNotifyManagerInterfaceV1 = m_server->attach<TreelandActiveNotifyManagerInterfaceV1>();
    m_keyboardShortcutsInhibitManagerV1 = m_server->attach<KeyboardShortcutsInhibitManagerV1>();

    // start() synchronously reports the initially available outputs. Their
    // per-output DConfig objects initialize asynchronously, so finish the scan
    // only after every reported output has either loaded its config or failed
    // initialization and fallen back to defaults.
    wlr_backend_start(m_backend->handle());
    m_outputManager->backendStarted();
}

SeatManager *Helper::seatManager() const
{
    return m_seatManager;
}

WSeat *Helper::getSeatForEvent(QInputEvent *event) const
{
    return m_seatManager->getSeatForEvent(event);
}

void Helper::activateSurface(SurfaceWrapper *wrapper,
                             Qt::FocusReason reason,
                             WSeat *seat,
                             bool raise)
{
    if (wrapper && wrapper->isIMCandidatePanel())
        return;

    if (wrapper && !wrapper->acceptKeyboardFocus()) {
        if (raise) {
            wrapper->stackToLast();
        }

        return;
    }

    // Plain activation: if the deepest modal is minimized, refuse to activate the parent
    // entirely. The user must explicitly unminimize the modal first (e.g., click it).
    SurfaceWrapper *originalWrapper = wrapper;
    if (wrapper) {
        if (SurfaceWrapper *modal = wrapper->findModal()) {
            if (modal != wrapper) {
                if (modal->workspaceId() != wrapper->workspaceId()
                    && wrapper->workspaceId() != -1) {
                    workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
                }

                if (modal->isMinimized()) {
                    qCCritical(lcTlShell) << "Refusing to activate parent with minimized modal"
                                          << "parent =" << wrapper << "modal =" << modal;
                    return;
                }

                originalWrapper->stackToLast();
                wrapper = modal;
            }
        }
    }

    if (m_blockActivateSurface && wrapper && wrapper->type() != SurfaceWrapper::Type::LockScreen) {
        if (wrapper->hasActiveCapability()) {
            workspace()->pushActivedSurface(wrapper);
        }
        return;
    }

    if (!wrapper || wrapper->hasActiveCapability()) {
        setActivatedSurface(wrapper, seat, raise);
    } else {
        qCCritical(lcTlShell)
            << "Trying to activate a surface which doesn't have ActiveCapability!";
    }

    if (!wrapper || wrapper->hasFocusCapability()) {
        requestKeyboardFocus(wrapper, reason, seat);
    }
}

void Helper::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (!wrapper) {
        qCCritical(lcTlShell) << "Don't force activate to empty surface! do you want `Helper::activeSurface(nullptr)`?";
        return;
    }
    if (!wrapper->shellSurface()) {
        qCWarning(lcTlShell) << "Try to force activate a destroyed surface!";
        return;
    }

    // Forced activation: the modal's minimized state is irrelevant — the caller explicitly
    // requested this surface. However, if the parent itself is minimized, it must be restored
    // here because the modal (which may already be unminimized) won't trigger parent linkage.
    SurfaceWrapper *originalWrapper = wrapper;
    const bool restoreAnimation =
        !(reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason);
    if (SurfaceWrapper *modal = wrapper->findModal()) {
        if (modal != wrapper) {
            if (modal->workspaceId() != wrapper->workspaceId() && wrapper->workspaceId() != -1) {
                workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
            }
            if (originalWrapper->isMinimized()) {
                originalWrapper->restoreFromMinimized(restoreAnimation);
            }
            originalWrapper->stackToLast();
            wrapper = modal;
        }
    }

    restoreFromShowDesktop(wrapper);

    if (wrapper->isMinimized()) {
        wrapper->restoreFromMinimized(restoreAnimation);
    }

    if (!wrapper->surface()->mapped()) {
        qCWarning(lcTlShell) << "Can't activate unmapped surface: " << wrapper;
        return;
    }

    if (!wrapper->showOnWorkspace(workspace()->current()->id()))
        workspace()->switchTo(workspace()->modelIndexOfSurface(wrapper));
    Helper::instance()->activateSurface(wrapper, reason, seat);
}

RootSurfaceContainer *Helper::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

WServer *Helper::server() const
{
    return m_server;
}

void Helper::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_primarySeat->setCursorPosition(position);
    Q_EMIT surface->resizeRequested(Qt::BottomEdge | Qt::RightEdge);
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *targetWindow, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend) {
        return false;
    }

    if (Q_UNLIKELY(!targetWindow || !event)) {
        return false;
    }

    if (event->isInputEvent()) {
        wlr_idle_notifier_v1_notify_activity(m_idleNotifier, seat->handle());

        // Only wake outputs disabled through output-power management.
        m_outputManager->wakePoweredOffOutputs();
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        const auto modifiers = kevent->modifiers();
        const auto ctrlAlt = Qt::ControlModifier | Qt::AltModifier;
        if ((modifiers & ctrlAlt) == ctrlAlt) {
            const auto key = kevent->key();
            if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
                const int vtnr = key - Qt::Key_F1 + 1;
                const bool sessionActive = m_backend->isSessionActive();
                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut received"
                                        << vtnr << "sessionActive" << sessionActive;
                if (!sessionActive) {
                    return true;
                }

                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut requested" << vtnr;
                wlr_session_change_vt(m_backend->session(), vtnr);
                return true;
            }
        }
    }

    WSeat *targetSeat = seat;
    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            targetSeat = m_seatManager->getSeatForDevice(device);
            if (!targetSeat) {
                qCWarning(lcTlCore) << "Device has no associated seat, using default seat";
                targetSeat = seat;
            }
        }
    }

    if (targetSeat && targetSeat != seat) {
        return false;
    }
    m_currentEventSeat = targetSeat;
    [[maybe_unused]] auto clearEventSeat = qScopeGuard([this] { m_currentEventSeat = nullptr; });

    // Dismiss the popup grab when the user presses a button outside the popup
    // (e.g. on the desktop or another client). wlroots' xdg popup keyboard grab
    // redirects keyboard focus back to the popup, so it must be ended explicitly.
    if (event->type() == QEvent::MouseButtonPress) {
        if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(targetSeat)) {
            if (seatContainer->hasPopupGrab()) {
                auto *focused = targetSeat->handle()->pointer_state.focused_surface;
                bool clickOnPopup = false;
                if (focused) {
                    if (auto *wSurface = WSurface::fromHandle(focused)) {
                        if (auto *wrapper = m_rootSurfaceContainer->getSurface(wSurface))
                            clickOnPopup = (wrapper->type() == SurfaceWrapper::Type::XdgPopup);
                    }
                }
                // seat->drag is non-null during an active DnD drag, which also
                // installs a keyboard grab; never end that grab here.
                if (!clickOnPopup && !targetSeat->handle()->drag)
                    seatContainer->dismissPopups();
            }
        }
    }
    if (seat == m_primarySeat) {
        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);
            switch (kevent->key()) {
            case Qt::Key_Meta:
            case Qt::Key_Super_L:
            case Qt::Key_Super_R:
                if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(true);
                break;
            default:
                if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(false);
                break;
            }
        }

        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);

#ifndef QT_NO_DEBUG
            if (QKeySequence(kevent->keyCombination()) ==
                QKeySequence(Qt::MetaModifier | Qt::Key_F12)) {
                std::terminate();
            }
            // The debug view shortcut should always handled first
            if (QKeySequence(kevent->keyCombination())
                == QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier | Qt::Key_F11)) {
                if (toggleDebugMenuBar())
                    return true;
            }
#endif

            if (m_captureSelector) {
                if (event->modifiers() == Qt::NoModifier && kevent->key() == Qt::Key_Escape)
                    m_captureSelector->cancelSelection();
            }
        }

        if (event->type() == QEvent::KeyRelease && !m_captureSelector) {
            auto kevent = static_cast<QKeyEvent *>(event);
            const int key = kevent->key();
            if (key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Shift
                || key == Qt::Key_Meta) {
                Q_EMIT modifierKeyReleased(kevent);
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

    if (m_currentMode != CurrentMode::LockScreen)
        doGesture(event);

    // Per-seat move/resize handling
    const auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);
    if (seatContainer && seatContainer->moveResizeState().surface) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            const auto &moveResizeState = seatContainer->moveResizeState();
            auto ownsOutput = moveResizeState.surface->ownsOutput();
            if (!ownsOutput) {
                m_rootSurfaceContainer->endMoveResizeForSeat(seat);
                return false;
            }

            auto increment_pos = ev->globalPosition() - moveResizeState.initialPosition;
            m_rootSurfaceContainer->doMoveResizeForSeat(seat, increment_pos);
            // Edge-tiling detection during move (resize is not tiled).
            if (moveResizeState.edges == Qt::Edges())
                m_rootSurfaceContainer->detectEdgeTilingForSeat(seat);

            return true;
        } else if (event->type() == QEvent::KeyPress
                   && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_rootSurfaceContainer->cancelMoveResizeForSeat(seat);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_rootSurfaceContainer->endMoveResizeForSeat(seat);
        }
    }

    // Suppress compositor shortcuts when a keyboard shortcuts inhibitor is active
    if (m_currentMode == CurrentMode::Normal) {
        auto *focusSurface = seat->keyboardFocusSurface();
        if (focusSurface
            && m_keyboardShortcutsInhibitManagerV1->isInhibited(seat->handle(),
                                                                focusSurface->handle()))
            return false;
    }

    // Capture mode: intercept key events before dispatchKeyEvent
    if (m_shortcutManager->isCaptureActive() && m_shortcutManager->tryHandleCaptureEvent(seat, event))
        return true;

    if (seat == m_primarySeat && !m_captureSelector && m_currentMode != CurrentMode::LockScreen
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto kevent = static_cast<QKeyEvent *>(event);
        auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);

        // Meta: consume press as modifier; suppress release when used in combo
        if (kevent->key() == Qt::Key_Meta || kevent->key() == Qt::Key_Super_L || kevent->key() == Qt::Key_Super_R) {
            if (kevent->type() == QEvent::KeyPress) {
                return true;
            }
            if (kevent->type() == QEvent::KeyRelease && seatContainer && !seatContainer->metaKeyPressed()) {
                return false;
            }
        }

        if (m_shortcutManager->controller()->dispatchKeyEvent(kevent)) {
            return true;
        }
    }

    return false;
}

bool Helper::afterHandleEvent([[maybe_unused]] WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend)
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem *>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_rootSurfaceContainer->getSurface(watched);
        WSeat *eventSeat = getSeatForEvent(event);

        if (eventSeat && surface) {
            updateSurfaceSeatInteraction(surface, eventSeat);
            activateSurface(surface, Qt::MouseFocusReason, eventSeat);
        }
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend)
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        WSeat *eventSeat = getSeatForEvent(event);
        activateSurface(nullptr, Qt::OtherFocusReason, eventSeat);
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

            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    InputDevice::instance()->processSwipeCancel();
                else
                    InputDevice::instance()->processSwipeEnd();
            }
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldEnd();
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

SurfaceWrapper *Helper::activatedSurface() const
{
    if (!m_rootSurfaceContainer)
        return nullptr;

    auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(m_primarySeat);
    return seatContainer ? seatContainer->activatedSurface() : nullptr;
}

void Helper::setActivatedSurface(SurfaceWrapper *newActivateSurface, WSeat *seat, bool raise)
{
    if (!m_rootSurfaceContainer) {
        qCWarning(lcTlCore) << "Cannot set activated surface: root surface container is null";
        return;
    }

    WSeat *targetSeat = seat ? seat : m_primarySeat;
    auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(targetSeat);
    if (!seatContainer) {
        qCWarning(lcTlCore) << "Cannot set activated surface: no seat container for seat"
                            << targetSeat;
        return;
    }

    if (seatContainer->activatedSurface() == newActivateSurface)
        return;

    const bool isPrimarySeat = (targetSeat == m_primarySeat);
    auto *oldPrimarySurface = isPrimarySeat ? activatedSurface() : nullptr;

    if (oldPrimarySurface)
        oldPrimarySurface->setActivate(false);

    bool wasShowingDesktop = false;

    if (newActivateSurface) {
        Q_ASSERT(newActivateSurface->showOnWorkspace(workspace()->current()->id()));
        if (raise) {
            newActivateSurface->stackToLast();
            if (newActivateSurface->type() == SurfaceWrapper::Type::XWayland) {
                auto xwaylandSurface =
                    qobject_cast<WXWaylandSurface *>(newActivateSurface->shellSurface());
                Q_ASSERT(!xwaylandSurface->isBypassManager());
                xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
            }
        }
    }

    if (newActivateSurface) {
        if (m_showDesktop == ShowDesktopInterfaceV1::State::Show) {
            cancelShowDesktop(newActivateSurface);
            newActivateSurface->setHideByShowDesk(true);
            wasShowingDesktop = true;
        }

        Q_ASSERT(newActivateSurface->hasActiveCapability());
        workspace()->pushActivedSurface(newActivateSurface);
    }

    // SeatSurfaceManager emits activatedSurfaceChanged, and RootSurfaceContainer forwards
    // it for the primary seat. Do not emit Helper::activatedSurfaceChanged again here.
    seatContainer->setActivatedSurface(newActivateSurface, Qt::OtherFocusReason);

    // This also restores keyboard focus on the other seats; the caller's subsequent
    // requestKeyboardFocus() only covers the target seat.
    if (wasShowingDesktop)
        restoreShowDesktopFocus();

    if (isPrimarySeat && newActivateSurface) {
        Q_ASSERT(newActivateSurface->hasActiveCapability());
        newActivateSurface->setActivate(true);
    }

}

void Helper::onRenderWindowActiveFocusItemChanged()
{
    if (!keyboardFocusSurface()) {
        // Keyboard focus moved to a non-client window (e.g. internal QML component).
        // Notify all seats to clear the keyboard focus surface.
        const auto seats = m_seatManager->seats();
        for (auto *seat : seats) {
            if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat)) {
                if (seatContainer->keyboardFocusSurface())
                    seatContainer->setKeyboardFocusSurface(nullptr);
            }
        }
    }
}

void Helper::requestKeyboardFocus(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (wrapper) {
        // Pop up through parent hierarchy until we find a non-popup surface or grabbed popup
        while (wrapper) {
            auto *popupSurface = qobject_cast<WXdgPopupSurface *>(wrapper->shellSurface());
            if (!popupSurface)
                break;
            if (popupSurface->handle()->seat)
                break;
            wrapper = wrapper->parentSurface();
        }
        if (!wrapper || !wrapper->hasFocusCapability()) {
            qCDebug(lcTlShell) << "Request keyboard focus for surface without focus capability!"
                                 << "surface =" << wrapper;
            return;
        }
    }

    if (!seat)
        seat = m_primarySeat;

    // Delegate to SeatSurfaceManager which handles keyboardFocusPriority checks,
    // Qt focus management, Wayland focus, multi-seat arbitration, and interaction metadata.
    auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);
    Q_ASSERT(seatContainer);
    seatContainer->setKeyboardFocusSurface(wrapper, reason);
}

void Helper::setCursorPosition(const QPointF &position)
{
    const auto seats = m_seatManager->seats();
    for (auto *seat : seats) {
        m_rootSurfaceContainer->endMoveResizeForSeat(seat);
    }
    m_primarySeat->setCursorPosition(position);
}

void Helper::handleRequestDrag([[maybe_unused]] WSurface *surface)
{
    m_primarySeat->setAlwaysUpdateHoverTarget(true);

    struct wlr_drag *drag = m_primarySeat->handle()->drag;
    Q_ASSERT(drag);
    dragDropListener.init(&drag->events.drop, this, [this] (void *) {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(m_primarySeat);
        ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Dropped, m_primarySeat);
    });

    dragDestroyListener.init(&drag->events.destroy, this, [this, drag] (void *) {
        // Detach both listeners before wlr_drag destroy asserts the drop and
        // destroy listener lists are empty after emitting destroy.
        dragDropListener.disconnect();
        dragDestroyListener.disconnect();
        if (!drag->dropped)
            ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Cancelled, m_primarySeat);
        drag->data = NULL;
        m_primarySeat->setAlwaysUpdateHoverTarget(false);
    });
    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(m_primarySeat);
    ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Started, m_primarySeat);
}

void Helper::handleLockScreen(LockScreenInterface *lockScreen)
{
    connect(lockScreen, &LockScreenInterface::shutdown, this, &Helper::showShutdownMenu);
    connect(lockScreen, &LockScreenInterface::lock, this, [this]() {
        if (isNormalOrMultitaskview())
            showLockScreen(false);
    });
    connect(lockScreen, &LockScreenInterface::switchUser, this, &Helper::showSwitchUser);
}

void Helper::handleCompositorAction(uint32_t action)
{
    const auto shellAction = CompositorActionInterfaceV1::mapCompositorAction(action);
    if (!shellAction) {
        // Mapped at the protocol boundary: unsupported and unknown actions are
        // ignored (never fatal), per protocol.
        qCDebug(lcTlProtocol) << "Ignoring unsupported compositor action:" << action;
        return;
    }

    ShellActionExecutor::execute(*shellAction);
}


void Helper::onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath)
{
    const auto path = sessionPath.path();
    qCDebug(lcTlCore) << "Session new, sessionId:" << sessionId << ", sessionPath:" << path;
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Lock", this, SLOT(onSessionLock()));
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Unlock", this, SLOT(onSessionUnlock()));
}

void Helper::onSessionLock()
{
    showLockScreen();
}

void Helper::onSessionUnlock()
{
    if (m_lockScreen) {
        m_lockScreen->unlock();
    }
}

void Helper::onExtSessionLock(WSessionLock *lock)
{
#ifdef EXT_SESSION_LOCK_V1
    if (m_lockScreen->isLocked()) {
        lock->finish();
        return;
    }

    m_lockScreen->onExternalLock(lock);

    prepareLockScreenTransition();

    QObject::connect(lock, &WSessionLock::abandoned, this, [this]() {
        m_lockScreenGraceTimer->stop();
        setNoAnimation(false);
    });

    QObject::connect(lock, &WSessionLock::canceled, this, [this]() {
        m_lockScreenGraceTimer->stop();
    });

    m_lockScreenGraceTimer->disconnect();
    // grace 300ms for possible client to
    connect(m_lockScreenGraceTimer, &QTimer::timeout, this, [this, lock]() {
        setNoAnimation(true);
        lock->lock();
    });
    m_lockScreenGraceTimer->start();
#endif
}

Output *Helper::getOutput(WOutput *output) const
{
    return m_outputManager->outputFor(output);
}

const QList<Output *> &Helper::outputs() const
{
    return m_outputManager->outputs();
}

void Helper::addOutput()
{
    m_outputManager->requestAdditionalOutputs();
}

void Helper::setOutputMode(OutputMode mode)
{
    m_outputManager->applyMode(
        mode == OutputMode::Copy ? OutputManager::Mode::Copy : OutputManager::Mode::Extension);
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
    return m_outputManager->mode() == OutputManager::Mode::Copy
        ? OutputMode::Copy
        : OutputMode::Extension;
}

/**
 * Add a WSocket to the Wayland server.
 * This function is used by Treeland::ActivateWayland.
 *
 * @param socket WSocket to add
 */
void Helper::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

bool Helper::toggleDebugMenuBar()
{
    bool ok = false;

    const auto outputs = rootSurfaceContainer()->outputs();
    if (outputs.isEmpty())
        return false;

    bool firstOutputDebugMenuBarIsVisible = false;
    if (auto menuBar = outputs.first()->debugMenuBar()) {
        firstOutputDebugMenuBarIsVisible = menuBar->isVisible();
    }

    for (const auto &output : outputs) {
        if (output->debugMenuBar()) {
            output->debugMenuBar()->setVisible(!firstOutputDebugMenuBarIsVisible);
            ok = true;
        }
    }

    return ok;
}

ShowDesktopInterfaceV1::State Helper::showDesktopState() const
{
    return m_showDesktop;
}

WXdgOutputManager *Helper::xwaylandOutputManager() const
{
    return m_outputManager->xwaylandOutputManager();
}

void Helper::setLaunchpadMapped(WOutput *output, bool mapped)
{
    Q_EMIT launchpadMappedChanged(output, mapped);
}

void Helper::showDesktop(WOutput *output)
{
    Q_EMIT showDesktopRequested(output);
}

void Helper::startLockscreen(WOutput *output, bool showAnimation)
{
    Q_EMIT startLockscreened(output, showAnimation);
}

QString Helper::currentWorkspaceWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentWorkspaceWallpaper(output);
}

QString Helper::currentLockScreenWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentLockScreenWallpaper(output);
}

void Helper::handleWindowPicker(WindowPickerInterface *picker)
{
    connect(picker, &WindowPickerInterface::pick, this, [this, picker](const QString &hint) {
        auto windowPicker =
            qobject_cast<WindowPicker *>(qmlEngine()->createWindowPicker(m_rootSurfaceContainer));
        windowPicker->setHint(hint);
        connect(windowPicker,
                &WindowPicker::windowPicked,
                this,
                [picker, windowPicker](WSurfaceItem *surfaceItem) {
                    if (surfaceItem) {
                        auto credentials = WClient::getCredentials(
                            surfaceItem->surface()->waylandClient()->handle());
                        picker->sendWindowPid(credentials->pid);
                        windowPicker->deleteLater();
                    }
                });
        connect(picker,
                &WindowPickerInterface::beforeDestroy,
                windowPicker,
                &WindowPicker::deleteLater);
    });
}

void Helper::setMultitaskViewImpl(IMultitaskView *impl)
{
    m_multitaskView = impl;
}

void Helper::setLockScreenImpl(ILockScreen *impl)
{
#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
    if (!impl) {
        if (m_lockScreen) {
            m_lockScreen = nullptr;
            delete m_lockScreen;
        }
        return;
    }

    m_lockScreen = new LockScreen(impl, m_rootSurfaceContainer, m_greeterProxy);
    m_lockScreen->setZ(RootSurfaceContainer::LockScreenZOrder);
    m_lockScreen->setObjectName(QStringLiteral("LockScreenContainer"));
    m_lockScreen->setVisible(false);

    m_greeterProxy->setLockScreen(m_lockScreen);

    for (auto *output : std::as_const(m_rootSurfaceContainer->outputs())) {
        m_lockScreen->addOutput(output);
    }

    connect(m_lockScreen, &LockScreen::unlock, this, [this] {
        setCurrentMode(CurrentMode::Normal);
        setWorkspaceVisible(true);
#ifdef EXT_SESSION_LOCK_V1
        setNoAnimation(false);
#endif
        if (auto *surface = activatedSurface()) {
            if (surface->hasFocusCapability()) {
                requestKeyboardFocus(surface, Qt::NoFocusReason);
            }
        }
    });
    if (!impl) {
        return;
    }
    if (CmdLine::ref().useLockScreen()) {
        // Start in the undecided state: make the lock screen surface (wallpaper)
        // visible but keep the login UI hidden until DDM decides (ShowGreeter /
        // UserActivateMessage) or the fallback timeout in GreeterProxy fires.
        m_lockScreen->setVisible(true);
    }
#else
    Q_UNUSED(impl)
#endif
}

void Helper::setCurrentMode(CurrentMode mode)
{
    if (m_currentMode == mode)
        return;

    setBlockActivateSurface(mode != CurrentMode::Normal);

    m_currentMode = mode;

    // Deactivate pointer constraints when leaving Normal mode (modal shell state).
    if (m_currentMode != CurrentMode::Normal && m_pointerConstraintsManager)
        m_pointerConstraintsManager->deactivateAll();

    Q_EMIT currentModeChanged();
}

void Helper::prepareLockScreenTransition()
{
    if (m_multitaskView) {
        m_multitaskView->immediatelyExit();
    }
    deleteTaskSwitch();
    setCurrentMode(CurrentMode::LockScreen);
    setWorkspaceVisible(false);
}

void Helper::showLockScreen(bool switchToGreeter)
{
    if (!isLockScreenAvailable()) {
        return;
    }
    // LockScreen::isLocked() is isVisible(), which is also true in the
    // undecided state (surface shown, not yet locked), so check the real
    // lock state instead.
    if (m_greeterProxy->isLocked()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->lock();

    if (switchToGreeter) {
        QThreadPool::globalInstance()->start([]() {
            QDBusInterface interface("org.freedesktop.DisplayManager",
                                     "/org/freedesktop/DisplayManager/Seat0",
                                     "org.freedesktop.DisplayManager.Seat",
                                     QDBusConnection::systemBus());
            interface.call("SwitchToGreeter");
        });
    }
}

bool Helper::isLockScreenAvailable() const
{
    return m_lockScreen && m_lockScreen->available();
}

void Helper::showShutdownMenu()
{
    if (!isLockScreenAvailable() || !isNormalOrMultitaskview()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->shutdown();
}

void Helper::showSwitchUser()
{
    if (!isLockScreenAvailable() || !isNormalOrMultitaskview()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->switchUser();
}

WSeat *Helper::seat() const
{
    return m_primarySeat;
}

void Helper::handleLeftButtonStateChanged(const QInputEvent *event)
{
    Q_ASSERT(m_primarySeat);
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_primarySeat;
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            DDEActiveInterface::sendActiveIn(DDEActiveInterface::Mouse, m_primarySeat);
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Active,
                                                seat);
        } else {
            DDEActiveInterface::sendActiveOut(DDEActiveInterface::Mouse, m_primarySeat);
            ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Mouse,
                                                ActiveNotifyV1::Inactive,
                                                seat);
        }
    }
}

void Helper::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_primarySeat);
    WSeat *seat = m_currentEventSeat ? m_currentEventSeat : m_primarySeat;
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        DDEActiveInterface::sendActiveOut(DDEActiveInterface::Wheel, m_primarySeat);
        ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Wheel,
                                            ActiveNotifyV1::Inactive,
                                            seat);
    }
    if (delta.x() + delta.y() > 0) {
        DDEActiveInterface::sendActiveIn(DDEActiveInterface::Wheel, m_primarySeat);
        ActiveNotifyV1::sendActivityChanged(ActiveNotifyV1::Wheel,
                                            ActiveNotifyV1::Active,
                                            seat);
    }
}

void Helper::cancelShowDesktop(SurfaceWrapper *excludeSurface)
{
    if (m_showDesktop != ShowDesktopInterfaceV1::State::Show)
        return;
    m_showDesktop = ShowDesktopInterfaceV1::State::Normal;
    m_showDesktopInterfaceV1->setDesktopState(ShowDesktopInterfaceV1::State::Normal);
    const auto &surfaces = m_outputManager->workspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface == excludeSurface)
            continue;
        if (!surface->isMinimized() && !surface->isVisible()) {
            surface->setHideByShowDesk(true);
            surface->minimize(/*onAnimation=*/ false);
        }
    }
}

void Helper::restoreFromShowDesktop(SurfaceWrapper *activeSurface)
{
    if (m_showDesktop != ShowDesktopInterfaceV1::State::Show)
        return;
    cancelShowDesktop(activeSurface);
    if (activeSurface) {
        activeSurface->restoreFromMinimized();
    }
    restoreShowDesktopFocus();
}

Output *Helper::getOutputAtCursor() const
{
    return m_outputManager->outputAtCursor();
}

void Helper::handleNewForeignToplevelCaptureRequest(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request)
{
    if (!request || !request->toplevel_handle) {
        qCWarning(lcTlCapture) << "Invalid capture request or toplevel handle";
        return;
    }

    auto *handle = request->toplevel_handle;
    WToplevelSurface *toplevelSurface = m_extForeignToplevelListV1->findSurfaceByHandle(handle);
    if (!toplevelSurface) {
        qCWarning(lcTlCapture) << "Could not find toplevel surface for handle";
        return;
    }

    SurfaceWrapper *surfaceWrapper = m_rootSurfaceContainer->getSurface(toplevelSurface);
    if (!surfaceWrapper) {
        qCWarning(lcTlCapture) << "Could not find SurfaceWrapper for toplevel surface";
        return;
    }

    WSurfaceItem *surfaceItem = surfaceWrapper->surfaceItem();
    if (!surfaceItem) {
        qCWarning(lcTlCapture) << "Could not get WSurfaceItem from SurfaceWrapper";
        return;
    }

    WSurfaceItemContent *surfaceContent = surfaceItem->findItemContent();
    if (!surfaceContent) {
        qCWarning(lcTlCapture) << "Could not find WSurfaceItemContent";
        return;
    }

    qCDebug(lcTlCapture) << "Found WSurfaceItemContent for capture:"
             << "size=" << surfaceContent->size()
             << "implicitSize=" << QSizeF(surfaceContent->implicitWidth(), surfaceContent->implicitHeight())
             << "isTextureProvider=" << surfaceContent->isTextureProvider();

    auto *output = surfaceWrapper->ownsOutput()->output();
    if (!output) {
        qCWarning(lcTlCapture) << "Could not get WOutput from SurfaceWrapper";
        return;
    }

    auto *imageCaptureSource = new WExtImageCaptureSourceV1Impl(surfaceContent, output);

    bool success = wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
        request, imageCaptureSource->handle());

    if (!success) {
        qCWarning(lcTlCapture) << "Failed to accept foreign toplevel image capture request";
        delete imageCaptureSource;
    }
}

DDMInterfaceV1 *Helper::ddmInterfaceV1() const {
    return m_ddmInterfaceV1;
}

bool Helper::activateUserSession(const QString &username, const QString &sessionId)
{
    if (!m_userModel->getUser(username))
        return false;

    const auto update = m_sessionManager->prepareActiveUserSession(username, sessionId);
    if (!update)
        return false;

    m_userModel->setCurrentUserName(username);
    m_sessionManager->commitActiveUserSession(update);
    return true;
}

void Helper::enableRender() {
    m_renderWindow->setRenderEnabled(true);
}

void Helper::disableRender() {
    m_renderWindow->setRenderEnabled(false);
}

void Helper::setBlockActivateSurface(bool block)
{
    if (block == m_blockActivateSurface)
        return;
    m_blockActivateSurface = block;
    Q_EMIT blockActivateSurfaceChanged();
}

bool Helper::blockActivateSurface() const
{
    return m_blockActivateSurface;
}

bool Helper::noAnimation() const {
    return m_noAnimation;
}

void Helper::setNoAnimation(bool noAnimation) {
    if (m_noAnimation == noAnimation)
        return;
    m_noAnimation = noAnimation;
    emit noAnimationChanged();
}

void Helper::toggleFpsDisplay()
{
    if (m_fpsDisplay) {
        m_fpsDisplay->deleteLater();
        m_fpsDisplay = nullptr;
        return;
    }

    m_fpsDisplay = qmlEngine()->createFpsDisplay(m_renderWindow->contentItem());
}

/**
 * Move a XWayland window's surface corresponding to wid, to a
 * position relative to a WSurface. Top-left point is always used.
 *
 * @param wid X Window ID for the XWayland surface
 * @param anchor The anchor WSurface to be relative to
 * @param dx Horizontal distance between the top-left point of anchor and the destination
 * @param dy Vertical distance between the top-left point of anchor and the destination
 */
bool Helper::setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const
{
    SurfaceWrapper *ach = m_rootSurfaceContainer->getSurface(anchor);
    if (!ach) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: Failed to get SurfaceWrapper from WSurface";
        return false;
    }

    SurfaceWrapper *target = nullptr;
    for (SurfaceWrapper *wrapper : std::as_const(rootSurfaceContainer()->surfaces())) {
        if (wrapper->type() == SurfaceWrapper::Type::XWayland) {
            wlr_xwayland_surface *surface =
                wlr_xwayland_surface_try_from_wlr_surface(wrapper->surface()->handle());
            if (surface && surface->window_id == static_cast<xcb_window_t>(wid)) {
                target = wrapper;
                break;
            }
        }
    }
    if (!target) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: XWayland surface corresponding to WID" << wid << "not found!";
        return false;
    }

    QRectF rect(ach->position(), target->size());
    rect.translate(wl_fixed_to_double(dx), wl_fixed_to_double(dy));

    // For XWayland surfaces, setting wrapper position while following
    // implicit surface position may get overwritten by feedback updates.
    // Temporarily switch to compositor-driven position so moveTo() sends
    // configure with the new coordinates.
    target->setXwaylandPositionFromSurface(false);
    target->setPosition(rect.topLeft());
    target->setXwaylandPositionFromSurface(true);
    return true;
}

WXWayland *Helper::createXWayland()
{
    return shellHandler()->createXWayland(m_server, m_primarySeat, m_compositor, false);
}

WSeat *Helper::findSeatForSurface(SurfaceWrapper *wrapper) const
{
    return getLastInteractingSeat(wrapper);
}

void Helper::handleRequestDragForSeat(WSeat *seat, WSurface *)
{
    if (!seat || !seat->handle())
        return;

    seat->setAlwaysUpdateHoverTarget(true);
    struct wlr_drag *drag = seat->handle()->drag;
    Q_ASSERT(drag);

    SeatDragEntry entry;
    entry.drag = drag;
    entry.owner = std::make_unique<WListenerOwner>();
    auto *dragOwner = entry.owner.get();
    seat->listeners(dragOwner)->add(&drag->events.drop, this, [this, seat] (void *) {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(seat);
        ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Dropped, seat);
    });

    QPointer<WCursor> dragCursor = seat->cursor();
    seat->listeners(dragOwner)->add(&drag->events.destroy, this, [this, seat, drag, dragCursor, dragOwner] (void *) {
        if (dragCursor)
            dragCursor->setOverrideCursor(WCursor::toQCursor(WGlobal::CursorShape::Invalid));
        if (!drag->dropped)
            ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Cancelled, seat);
        drag->data = NULL;
        seat->setAlwaysUpdateHoverTarget(false);
        // Drop the drop/destroy/dnd_action listener entries while the destroy
        // signal is being emitted: wlr_drag.c drag_destroy asserts the drop
        // and destroy lists are empty after (erasing the entry destroys its
        // WScopedListenerList and disconnects all of them).
        seat->removeListeners(dragOwner);
        std::erase_if(seatDragEntries, [drag](const auto &e) {
            return e.drag == drag;
        });
    });

    // TODO: https://gitlab.freedesktop.org/wayland/wayland/-/work_items/444
    const auto updateCursor = [drag, dragCursor] {
        if (!dragCursor || drag->grab_type != WLR_DRAG_GRAB_KEYBOARD_POINTER)
            return;

        const auto action = drag->source
                ? drag->source->current_dnd_action
                : WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
        auto shape = WGlobal::CursorShape::NoDrop;
        if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
            shape = WGlobal::CursorShape::Copy;
        else if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
            shape = WGlobal::CursorShape::Move;
        else if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
            shape = WGlobal::CursorShape::DndAsk;
        dragCursor->setOverrideCursor(WCursor::toQCursor(shape));
    };

    if (drag->source) {
        seat->listeners(dragOwner)->add(&drag->source->events.dnd_action, this, updateCursor);
    }
    updateCursor();

    // Move the bundle into the list only after dndAction/updateCursor above.
    seatDragEntries.push_back(std::move(entry));

    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(seat);
    ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Started, seat);
}

WSeat *Helper::getLastInteractingSeat(SurfaceWrapper *surface) const
{
    if (!surface) {
        return nullptr;
    }

    auto lastSeatVariant = surface->property("lastInteractingSeat");
    if (lastSeatVariant.isValid()) {
        auto seat = lastSeatVariant.value<WSeat*>();
        if (m_seatManager->seats().contains(seat)) {
            return seat;
        }
    }
    return nullptr;
}

void Helper::updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat)
{
    if (!surface || !seat)
        return;

    surface->setProperty("lastInteractingSeat", QVariant::fromValue(seat));
    surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
}

void Helper::switchWorkspaceForSeat(WSeat *seat, int index)
{
    if (!seat)
        return;
    workspace()->switchTo(index);
}
