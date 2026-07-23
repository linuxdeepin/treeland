// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surface/surfacewrapper.h"

#include "common/treelandlogging.h"
#include "core/qmlengine.h"
#include "output/output.h"
#include "seat/helper.h"
#include "treelanduserconfig.hpp"
#include "workspace/workspace.h"
#include "wtoplevelsurface.h"

#include <winputpopupsurfaceitem.h>
#include <wlayersurface.h>
#include <wlayersurfaceitem.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wsocket.h>
#include <wxdgpopupsurfaceitem.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgtoplevelsurfaceitem.h>
#include <wxwaylandsurface.h>
#include <wxwaylandsurfaceitem.h>

#include <qwbuffer.h>
#include <qwlayershellv1.h>

#include <QColor>
#include <QTimer>
#include <QVariant>

#define OPEN_ANIMATION 1
#define CLOSE_ANIMATION 2
#define ALWAYSONTOPLAYER 1

namespace {
constexpr int initialMaximizeCommitTimeoutMs = 1500;
constexpr qreal initialMaximizeSizeTolerance = 2.0;
constexpr int defaultNormalWidth = 800;
constexpr int defaultNormalHeight = 600;

bool sizesMatch(const QSizeF &first, const QSizeF &second)
{
    return qAbs(first.width() - second.width()) <= initialMaximizeSizeTolerance
        && qAbs(first.height() - second.height()) <= initialMaximizeSizeTolerance;
}
} // namespace

SurfaceWrapper::SurfaceWrapper(QmlEngine *qmlEngine,
                               WToplevelSurface *shellSurface,
                               Type type,
                               const QString &appId,
                               QQuickItem *parent)
    : SurfaceWrapper(qmlEngine, shellSurface, type, appId, parent, {})
{
}

SurfaceWrapper::SurfaceWrapper(QmlEngine *qmlEngine,
                               WToplevelSurface *shellSurface,
                               Type type,
                               const QString &appId,
                               QQuickItem *parent,
                               const QRectF &initialMaximizedGeometry)
    : QQuickItem(parent)
    , m_engine(qmlEngine)
    , m_shellSurface(shellSurface)
    , m_type(type)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_noTitleBar(true)
    , m_noCornerRadius(false)
    , m_alwaysOnTop(false)
    , m_skipSwitcher(false)
    , m_skipDockPreView(true)
    , m_skipMutiTaskView(false)
    , m_isDdeShellSurface(false)
    , m_xwaylandPositionFromSurface(true)
    , m_wrapperAboutToRemove(false)
    , m_isProxy(false)
    , m_hideByWorkspace(false)
    , m_hideByshowDesk(true)
    , m_hideByLockScreen(false)
    , m_confirmHideByLockScreen(false)
    , m_blur(false)
    , m_isActivated(false)
    , m_attention(false)
    , m_isIMCandidatePanel(false)
    , m_resizable(false)
    , m_maximizable(false)
    , m_modal(false)
    , m_appId(appId)
{
    QQmlEngine::setContextForObject(this, qmlEngine->rootContext());

    if (m_type == Type::XdgToplevel && initialMaximizedGeometry.isValid()
        && !initialMaximizedGeometry.isEmpty()) {
        m_initialMaximizePending = true;
        m_initialMaximizeConfigured = true;
        m_initialMaximizeGeometry = initialMaximizedGeometry;
        m_maximizedGeometry = initialMaximizedGeometry;
    }

    setup();
}

SurfaceWrapper::SurfaceWrapper(SurfaceWrapper *original, QQuickItem *parent)
    : QQuickItem(parent)
    , m_engine(original->m_engine)
    , m_shellSurface(original->m_shellSurface)
    , m_type(original->m_type)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_noTitleBar(true)
    , m_noCornerRadius(false)
    , m_alwaysOnTop(false)
    , m_skipSwitcher(false)
    , m_skipDockPreView(true)
    , m_skipMutiTaskView(false)
    , m_isDdeShellSurface(false)
    , m_xwaylandPositionFromSurface(true)
    , m_wrapperAboutToRemove(false)
    , m_isProxy(true)
    , m_hideByWorkspace(false)
    , m_hideByshowDesk(true)
    , m_hideByLockScreen(false)
    , m_confirmHideByLockScreen(false)
    , m_blur(false)
    , m_isActivated(false)
    , m_attention(false)
    , m_isIMCandidatePanel(false)
    , m_resizable(false)
    , m_maximizable(false)
    , m_modal(false)
    , m_appId(original->m_appId)
{
    QQmlEngine::setContextForObject(this, m_engine->rootContext());

    if (original->m_shellSurface) {
        setup();
    } else {
        setImplicitSize(original->implicitWidth(), original->implicitHeight());
        auto iconVar = original->prelaunchSplash()
            ? original->prelaunchSplash()->property("iconBuffer")
            : QVariant();
        QColor bgColor = original->prelaunchSplash()
            ? original->prelaunchSplash()->property("backgroundColor").value<QColor>()
            : QColor("#ffffff");

        m_prelaunchSplash =
            m_engine->createPrelaunchSplash(this,
                                            original->radius(),
                                            iconVar.value<QW_NAMESPACE::qw_buffer *>(),
                                            bgColor);
        setNoDecoration(false);

        connect(original, &SurfaceWrapper::surfaceItemCreated, this, [this, original]() {
            m_shellSurface = original->m_shellSurface;
            m_type = original->m_type;
            if (m_prelaunchSplash) {
                m_prelaunchSplash->deleteLater();
                m_prelaunchSplash = nullptr;
            }
            setup();
        });
    }
}

// Constructor used for the prelaunch splash
SurfaceWrapper::SurfaceWrapper(QmlEngine *qmlEngine,
                               QQuickItem *parent,
                               const QSize &initialSize,
                               const QString &appId,
                               QW_NAMESPACE::qw_buffer *iconBuffer,
                               const QColor &backgroundColor)
    : QQuickItem(parent)
    , m_engine(qmlEngine)
    , m_shellSurface(nullptr)
    , m_type(Type::SplashScreen)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_noTitleBar(true)
    , m_noCornerRadius(false)
    , m_alwaysOnTop(false)
    , m_skipSwitcher(false)
    , m_skipDockPreView(false)
    , m_skipMutiTaskView(false)
    , m_isDdeShellSurface(false)
    , m_xwaylandPositionFromSurface(true)
    , m_wrapperAboutToRemove(false)
    , m_isProxy(false)
    , m_hideByWorkspace(false)
    , m_hideByshowDesk(true)
    , m_hideByLockScreen(false)
    , m_confirmHideByLockScreen(false)
    , m_blur(false)
    , m_isActivated(false)
    , m_attention(false)
    , m_isIMCandidatePanel(false)
    , m_resizable(false)
    , m_maximizable(false)
    , m_modal(false)
    , m_appId(appId)
{
    QQmlEngine::setContextForObject(this, qmlEngine->rootContext());
    const QSize restoredSize =
        initialSize.isValid() && initialSize.width() > 0 && initialSize.height() > 0
        ? initialSize
        : QSize(defaultNormalWidth, defaultNormalHeight);
    if (restoredSize == initialSize) {
        // Also set implicit size to keep QML layout consistent
        qCDebug(lcTlSurface) << "Prelaunch Splash: set initial size to" << restoredSize;
    }
    setImplicitSize(restoredSize.width(), restoredSize.height());
    setRestoredNormalSize(restoredSize);
    m_prelaunchSplash =
        m_engine->createPrelaunchSplash(this, radius(), iconBuffer, backgroundColor);

    setNoDecoration(false);
    updateHasActiveCapability(ActiveControlState::HasActivateCapability, true);
    updateHasActiveCapability(ActiveControlState::MappedOrSplash, true); // Splash is true
}

void SurfaceWrapper::invalidate()
{
    Q_ASSERT_X(!m_wrapperAboutToRemove, Q_FUNC_INFO, "Can't call `invalidate` twice!");
    m_wrapperAboutToRemove = true;
    m_initialMaximizePending = false;
    m_initialMaximizeConfigured = false;
    ++m_initialMaximizeGeneration;
    Q_EMIT aboutToBeInvalidated();

    if (!m_skipDockPreView)
        setSkipDockPreView(true);

    if (m_container) {
        m_container->removeSurface(this);
        m_container = nullptr;
    }
    if (m_ownsOutput) {
        m_ownsOutput->removeSurface(this);
        m_ownsOutput = nullptr;
    }
    if (m_parentSurface) {
        m_parentSurface->removeSubSurface(this);
        m_parentSurface = nullptr;
    }
    for (auto subS : std::as_const(m_subSurfaces)) {
        subS->m_parentSurface = nullptr;
    }

    updateHasActiveCapability(ActiveControlState::MappedOrSplash, false);
    updateFocusControlState(FocusControlState::Mapped, false);

    m_subSurfaces.clear();
    m_shellSurface = nullptr;
    if (m_surfaceItem)
        m_surfaceItem->disconnect(this);
}

SurfaceWrapper::~SurfaceWrapper()
{
    if (!m_wrapperAboutToRemove) {
        if (isWindowAnimationRunning()) {
            qCWarning(lcTlSurface)
                << "SurfaceWrapper is being destroyed without destroy(); expected external"
                   " destroy() rather than QObject parent-child destruction";
        }
        invalidate();
    }
    if (m_titleBar) {
        delete m_titleBar;
        m_titleBar = nullptr;
    }
    if (m_decoration) {
        delete m_decoration;
        m_decoration = nullptr;
    }
    if (m_geometryAnimation) {
        delete m_geometryAnimation;
        m_geometryAnimation = nullptr;
    }
    if (m_windowAnimation) {
        delete m_windowAnimation;
        m_windowAnimation = nullptr;
    }
    if (m_coverContent) {
        delete m_coverContent;
        m_coverContent = nullptr;
    }
    // Ensure we do not hold stale connections to shellSurface; QPointer may already be null.
    if (m_shellSurface) {
        QObject::disconnect(m_shellSurface, nullptr, this, nullptr);
        m_shellSurface.clear();
    }
}

void SurfaceWrapper::setup()
{
    Q_ASSERT(m_shellSurface);
    Q_ASSERT(m_type != Type::SplashScreen);

    const auto initialMaximizeRequested = [this] {
        if (m_isProxy)
            return false;

        const auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(m_shellSurface);
        return xwaylandSurface && xwaylandSurface->isMaximizeRequested();
    };
    // XWayland negotiates its initial state after map. Native XDG initial state is prepared by
    // ShellHandler during the initial empty commit and may already be armed before setup().
    if (!hasConfiguredInitialXdgMaximize())
        m_initialMaximizePending = initialMaximizeRequested();

    updateActivateCapability();
    updateFocusCapability();

    switch (m_type) {
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
        connect(m_surfaceItem,
                &WSurfaceItem::bufferScaleChanged,
                this,
                &SurfaceWrapper::updateSurfaceSizeRatio);
        updateSurfaceSizeRatio();
        break;
    }
    case Type::InputPopup:
        m_surfaceItem = new WInputPopupSurfaceItem(this);
        break;
#ifdef EXT_SESSION_LOCK_V1
    case Type::LockScreen:
        m_surfaceItem = new WSurfaceItem(this);
        break;
#endif
    default:
        Q_UNREACHABLE();
    }

    if (m_initialMaximizePending)
        m_surfaceItem->setVisible(false);

    QQmlEngine::setContextForObject(m_surfaceItem, m_engine->rootContext());
    m_surfaceItem->setDelegate(m_engine->surfaceContentComponent());
    m_surfaceItem->setResizeMode(WSurfaceItem::ManualResize);
    m_surfaceItem->setShellSurface(m_shellSurface);
    connect(m_surfaceItem, &WSurfaceItem::readyChanged, this, [this] {
        if (m_surfaceItem->isReady()) {
            tryApplyInitialMaximize();
            tryApplyDeferredSurfaceState();
        }
    });
    if (m_type == Type::XWayland) {
        connect(this, &QQuickItem::visibleChanged, this, [this] {
            if (isVisible())
                tryApplyDeferredSurfaceState();
        });
        connect(m_surfaceItem, &QQuickItem::visibleChanged, this, [this] {
            if (m_surfaceItem->isVisible())
                tryApplyDeferredSurfaceState();
        });
    }
    // Initialize focus policy even if focus capability state never toggles later.
    m_surfaceItem->setFocusPolicy(hasFocusCapability() ? Qt::StrongFocus : Qt::NoFocus);

    if (!m_isProxy) {
        m_shellSurface->safeConnect(&WToplevelSurface::requestMinimize, this, [this]() {
            minimize();
        });
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelMinimize, this, [this]() {
            restoreFromMinimized();
        });
        m_shellSurface->safeConnect(&WToplevelSurface::requestMaximize,
                                    this,
                                    &SurfaceWrapper::maximize);
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelMaximize,
                                    this,
                                    &SurfaceWrapper::unmaximize);
        m_shellSurface->safeConnect(&WToplevelSurface::requestMove, this, [this]() {
            Q_EMIT moveRequested();
        });
        m_shellSurface->safeConnect(&WToplevelSurface::requestResize,
                                    this,
                                    [this](WSeat *, Qt::Edges edge, quint32) {
                                        Q_EMIT resizeRequested(edge);
                                    });
        m_shellSurface->safeConnect(&WToplevelSurface::requestFullscreen,
                                    this,
                                    &SurfaceWrapper::enterFullscreen);
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelFullscreen,
                                    this,
                                    &SurfaceWrapper::leaveFullscreen);

        if (m_type == Type::XdgToplevel) {
            m_shellSurface->safeConnect(&WToplevelSurface::requestShowWindowMenu,
                                        this,
                                        [this](WSeat *, QPoint pos, quint32) {
                                            Q_EMIT windowMenuRequested(
                                                m_surfaceItem->mapFromSurface(pos).toPoint());
                                        });
        }
    }
    m_shellSurface->surface()->safeConnect(&WSurface::mappedChanged,
                                           this,
                                           &SurfaceWrapper::onMappedChanged);
    m_shellSurface->surface()->safeConnect(&WSurface::commit,
                                           this,
                                           [this](quint32) {
                                               if (!m_initialMaximizePending
                                                   || !m_initialMaximizeConfigured) {
                                                   return;
                                               }
                                               // WSurfaceItem observes the native commit
                                               // separately. Check after all direct commit
                                               // handlers have refreshed its implicit size.
                                               QTimer::singleShot(
                                                   0,
                                                   this,
                                                   &SurfaceWrapper::handleInitialMaximizeCommit);
                                           });

    Q_EMIT surfaceItemCreated();

    // Forward WToplevelSurface appId changes when using fallback mode
    if (m_appId.isEmpty()) {
        m_shellSurface->safeConnect(&WToplevelSurface::appIdChanged, this, [this]() {
            Q_EMIT appIdChanged();
        });
    }

    if (m_type == Type::XdgToplevel || m_type == Type::XWayland) {
        m_shellSurface->safeConnect(&WToplevelSurface::minimumSizeChanged,
                                    this,
                                    &SurfaceWrapper::updateSizeCapabilities);
        m_shellSurface->safeConnect(&WToplevelSurface::maximumSizeChanged,
                                    this,
                                    &SurfaceWrapper::updateSizeCapabilities);
    }
    updateSizeCapabilities();
    if (m_initialMaximizePending && !isMaximizable()) {
        const bool cancelInitialXdgConfigure = hasConfiguredInitialXdgMaximize();
        m_initialMaximizePending = false;
        m_initialMaximizeConfigured = false;
        ++m_initialMaximizeGeneration;
        if (cancelInitialXdgConfigure && m_shellSurface->isInitialized()) {
            m_shellSurface->resize(QSize());
            m_shellSurface->setMaximize(false);
        }
        if (!m_prelaunchSplash)
            m_surfaceItem->setVisible(true);
    }

    if (!m_prelaunchSplash) {
        setImplicitSize(m_surfaceItem->implicitWidth(), m_surfaceItem->implicitHeight());
        connect(m_surfaceItem, &WSurfaceItem::implicitWidthChanged, this, [this] {
            setImplicitWidth(m_surfaceItem->implicitWidth());
        });
        connect(m_surfaceItem, &WSurfaceItem::implicitHeightChanged, this, [this] {
            setImplicitHeight(m_surfaceItem->implicitHeight());
        });
        connect(m_surfaceItem,
                &WSurfaceItem::boundingRectChanged,
                this,
                &SurfaceWrapper::updateBoundingRect);
    }

    if (auto client = m_shellSurface->waylandClient()) {
        connect(client->socket(),
                &WSocket::enabledChanged,
                this,
                &SurfaceWrapper::onSocketEnabledChanged);
        onSocketEnabledChanged();
    }

    if (m_type == Type::XdgToplevel && !m_isProxy) // x11 will set later
        setSkipDockPreView(false);

    if (m_type == Type::Layer) {
        if (auto *layerSurface = qobject_cast<WLayerSurface *>(m_shellSurface.data())) {
            connect(layerSurface,
                    &WLayerSurface::keyboardInteractivityChanged,
                    this,
                    &SurfaceWrapper::updateFocusCapability);
        }
    }

    if (m_type == Type::XWayland && !m_isProxy) {
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(m_shellSurface);
        auto xwaylandSurfaceItem = qobject_cast<WXWaylandSurfaceItem *>(m_surfaceItem);

        connect(xwaylandSurfaceItem,
                &WXWaylandSurfaceItem::implicitPositionChanged,
                this,
                [this, xwaylandSurfaceItem]() {
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
            // NOTE: need a better check whether set positionAutomatic, WindowTypes?
            m_positionAutomatic = false;
            moveNormalGeometryInOutput(xwaylandSurfaceItem->implicitPosition());
        }

        auto updateX11SkipFlags = [this]() {
            // TODO: support missing type in waylib
            // https://github.com/linuxdeepin/dde-shell/blob/b94a3cec4e01cba64f56e040e74419248985d03d/panels/dock/taskmanager/x11window.cpp#L81-L107
            auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(this->shellSurface());
            if (!xwaylandSurface)
                return;
            auto atoms = xwaylandSurface->windowTypes();
            bool skipDock = false;
            skipDock |= xwaylandSurface->isBypassManager();
            // skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::_NET_WM_WINDOW_TYPE_DIALOG)
            // && !isActionMinimizeAllowed();
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_UTILITY);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_COMBO);
            // skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_DESKTOP);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_DND);
            // skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_DOCK);
            skipDock |=
                atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_DROPDOWN_MENU);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_MENU);
            skipDock |=
                atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_NOTIFICATION);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_POPUP_MENU);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_SPLASH);
            // skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_TOOLBAR);
            skipDock |= atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_TOOLTIP);
            setSkipDockPreView(skipDock);

            bool notSkipSwitcher = false;
            notSkipSwitcher |=
                atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_NORMAL);
            notSkipSwitcher |=
                atoms.testFlag(WXWaylandSurface::WindowType::NET_WM_WINDOW_TYPE_DIALOG);
            setSkipSwitcher(!notSkipSwitcher);
            setSkipMutiTaskView(!notSkipSwitcher);
        };

        connect(xwaylandSurface,
                &WXWaylandSurface::bypassManagerChanged,
                this,
                [this, updateX11SkipFlags]() {
                    updateX11SkipFlags();
                    updateSizeCapabilities();
                    updateActivateCapability();
                    updateFocusCapability();
                });
        connect(xwaylandSurface,
                &WXWaylandSurface::windowTypesChanged,
                this,
                [this, updateX11SkipFlags]() {
                    updateX11SkipFlags();
                    updateSizeCapabilities();
                });
        updateX11SkipFlags();
    }
    // Connect DConfig windowRadius change so QML bindings re-evaluate radius()
    if (m_type == Type::XdgToplevel || m_type == Type::XWayland) {
        if (auto *helper = Helper::instance()) {
            if (auto *config = helper->config()) {
                connect(config,
                        &TreelandUserConfig::windowRadiusChanged,
                        this,
                        &SurfaceWrapper::radiusChanged);
            }
        }
    }

    updateActivateCapability();
    updateFocusCapability();
    if (m_prelaunchSplash) {
        m_surfaceItem->setVisible(false);

        // Check if surface is still valid before accessing
        WSurface *surf = surface();
        if (surf && surf->mapped()) {
            updateFocusControlState(FocusControlState::Mapped, true);
            // ActiveControlState::MappedOrSplash is already true
            syncPrelaunchMappedState();
        }
    } else {
        updateFocusControlState(FocusControlState::Mapped, surface() && surface()->mapped());
        updateHasActiveCapability(ActiveControlState::MappedOrSplash,
                                  surface() && surface()->mapped());
    }

    // Also cover a request which arrived while setup() was connecting the surface.
    if (!m_initialMaximizePending && isMaximizable() && initialMaximizeRequested())
        m_initialMaximizePending = true;
    if (m_initialMaximizePending) {
        m_surfaceItem->setVisible(false);
        qCDebug(lcTlSurface) << "Detected initial maximize request for" << appId() << "type"
                             << static_cast<int>(m_type);
    }

    if (hasConfiguredInitialXdgMaximize())
        activateConfiguredInitialXdgMaximize();

    tryApplyInitialMaximize();
    if (m_prelaunchSplash && surface() && surface()->mapped())
        startPrelaunchSplashHideSequence();
}

void SurfaceWrapper::convertToNormalSurface(WToplevelSurface *shellSurface,
                                            Type type,
                                            const QRectF &initialMaximizedGeometry)
{
    // Conversion only allowed from prelaunch (SplashScreen) state
    if (m_type != Type::SplashScreen || m_shellSurface != nullptr) {
        qCCritical(lcTlSurface)
            << "convertToNormalSurface can only be called on prelaunch surfaces";
        return;
    }

    // Assign new shell surface (QPointer auto-detects destruction)
    m_shellSurface = shellSurface;
    m_type = type;
    if (m_type == Type::XdgToplevel && initialMaximizedGeometry.isValid()
        && !initialMaximizedGeometry.isEmpty()) {
        m_initialMaximizePending = true;
        m_initialMaximizeConfigured = true;
        m_initialMaximizeGeometry = initialMaximizedGeometry;
        m_maximizedGeometry = initialMaximizedGeometry;
    }
    Q_EMIT typeChanged();

    // Call setup() to initialize surfaceItem related features
    setup();
}

void SurfaceWrapper::adoptInitialXdgMaximize(const QRectF &targetGeometry)
{
    if (m_type != Type::XdgToplevel || !targetGeometry.isValid()
        || targetGeometry.isEmpty()) {
        return;
    }

    m_initialMaximizePending = true;
    m_initialMaximizeConfigured = true;
    m_initialMaximizeGeometry = targetGeometry;
    if (!m_maximizedGeometry.isValid() || m_maximizedGeometry.isEmpty())
        m_maximizedGeometry = targetGeometry;

    activateConfiguredInitialXdgMaximize();
    tryApplyInitialMaximize();
}

void SurfaceWrapper::setParent(QQuickItem *item)
{
    QObject::setParent(item);
    setParentItem(item);
}

void SurfaceWrapper::setActivate(bool activate)
{
    if (m_wrapperAboutToRemove)
        return;
    if (m_isActivated == activate)
        return;

    Q_ASSERT(!activate || hasActiveCapability());
    m_isActivated = activate;

    if (m_attention && m_isActivated)
        setAttention(false);

    // No shellSurface in prelaunch mode -> early return
    if (m_shellSurface)
        updateActiveState();

    Q_EMIT isActivatedChanged();
}

void SurfaceWrapper::updateActiveState()
{
    if (!m_shellSurface) {
        qCCritical(lcTlSurface) << "updateActiveState called without a valid shellSurface";
        return;
    }
    if (!m_shellSurface->isInitialized()) {
        qCWarning(lcTlSurface)
            << "updateActiveState called with shellSurface not yet initialized";
        return;
    }
    m_shellSurface->setActivate(m_isActivated);
}

void SurfaceWrapper::setFocus(bool focus, Qt::FocusReason reason)
{
    // No surfaceItem in prelaunch mode -> early return
    if (!m_surfaceItem) {
        qCDebug(lcTlSurface) << "setFocus called but m_surfaceItem is null, appId:" << m_appId;
        return;
    }

    if (focus)
        m_surfaceItem->forceActiveFocus(reason);
    else
        m_surfaceItem->setFocus(false, reason);
}

void SurfaceWrapper::syncPrelaunchMappedState()
{
    if (!m_prelaunchOutputs.isEmpty()) {
        setOutputs(m_prelaunchOutputs);
        m_prelaunchOutputs.clear();
    }

    updateActiveState();
}

void SurfaceWrapper::startPrelaunchSplashHideSequence()
{
    Q_ASSERT(m_surfaceItem != nullptr);
    if (m_initialMaximizePending) {
        tryApplyInitialMaximize();
        return;
    }
    if (m_windowAnimation) {
        qCDebug(lcTlSurface) << "prelaunch splash transition is starting while window "
                                    "animation is still running,"
                                    "this may cause visual glitches, will delay the transition "
                                    "until window animation finishes";
        return;
    }
    if (m_geometryAnimation) {
        qCDebug(lcTlSurface)
            << "Prelaunch splash transition deferred until geometry animation finishes";
        return;
    }

    // Wait until surfaceItem has computed a valid scene-space implicit size.
    // For XWayland, this happens in updateSurfaceState() after the first surface commit;
    // for other types it may be deferred until componentComplete + first polish.
    if (!m_surfaceItem->isReady()) {
        connect(m_surfaceItem,
                &WSurfaceItem::readyChanged,
                this,
                &SurfaceWrapper::startPrelaunchSplashHideSequence,
                Qt::SingleShotConnection);
        return;
    }

    // Use surfaceItem's scene-space implicit size: for XWayland, surf->size() is
    // buffer-space and differs from scene-space after DPR scaling via surfaceSizeRatio.
    const QSizeF targetImplicitSize(m_surfaceItem->implicitWidth(),
                                    m_surfaceItem->implicitHeight());
    const bool hasValidTargetImplicitSize =
        targetImplicitSize.width() > 0 && targetImplicitSize.height() > 0;
    if (!hasValidTargetImplicitSize) {
        qCCritical(lcTlSurface) << "Invalid target implicit size, skip transition animation"
                                    << "targetImplicit=" << targetImplicitSize;
    }

    if (hasValidTargetImplicitSize && !captureNormalGeometryFromSurfaceItem(false)) {
        qCWarning(lcTlSurface) << "Failed to capture client normal geometry for prelaunch surface"
                               << appId() << "targetImplicit=" << targetImplicitSize;
    }

    const bool needImplicitSizeTransition = hasValidTargetImplicitSize && (container() != nullptr)
        && (!qFuzzyCompare(implicitWidth() + 1.0, targetImplicitSize.width() + 1.0)
            || !qFuzzyCompare(implicitHeight() + 1.0, targetImplicitSize.height() + 1.0));

    if (needImplicitSizeTransition) {
        const QRectF fromGeometry(position(), size());
        const QRectF toGeometry = m_normalGeometry;
        m_geometryAnimation =
            m_engine->createGeometryAnimation(this, fromGeometry, toGeometry, container());

        bool ok = connect(m_geometryAnimation,
                          SIGNAL(ready()),
                          this,
                          SLOT(onPrelaunchGeometryAnimationReady()));
        Q_ASSERT(ok);
        ok = connect(m_geometryAnimation,
                     SIGNAL(finished()),
                     this,
                     SLOT(onPrelaunchGeometryAnimationFinished()));
        Q_ASSERT(ok);

        ok = QMetaObject::invokeMethod(m_geometryAnimation, "start");
        Q_ASSERT(ok);
    } else {
        completeSplashTransition(targetImplicitSize);
    }
}

void SurfaceWrapper::onPrelaunchGeometryAnimationReady()
{
    Q_ASSERT(m_geometryAnimation);
    const QRectF toGeo = m_geometryAnimation->property("toGeometry").toRectF();
    // Move the wrapper to the animation's final position before revealing the real surface,
    // so the window appears exactly where the animation ended (no position jump).
    setPosition(toGeo.topLeft());
    // Keep normalGeometry in sync so subsequent state transitions use the correct position.
    setNormalGeometryFromSurface(toGeo);

    completeSplashTransition(toGeo.size(), true);
}

void SurfaceWrapper::onPrelaunchGeometryAnimationFinished()
{
    Q_ASSERT(m_geometryAnimation);
    const QPointer<QQuickItem> finishedAnimation = m_geometryAnimation;
    m_geometryAnimation->disconnect(this);
    m_geometryAnimation->deleteLater();
    m_geometryAnimation = nullptr;

    if (m_decoration)
        m_decoration->setVisible(true);

    continuePendingTransitionsAfterAnimation(finishedAnimation);
}

void SurfaceWrapper::completeSplashTransition(const QSizeF &targetImplicitSize, bool hideDecoration)
{
    setImplicitSize(targetImplicitSize.width(), targetImplicitSize.height());
    connect(m_surfaceItem, &WSurfaceItem::implicitWidthChanged, this, [this] {
        setImplicitWidth(m_surfaceItem->implicitWidth());
    });
    connect(m_surfaceItem, &WSurfaceItem::implicitHeightChanged, this, [this] {
        setImplicitHeight(m_surfaceItem->implicitHeight());
    });
    connect(m_surfaceItem,
            &WSurfaceItem::boundingRectChanged,
            this,
            &SurfaceWrapper::updateBoundingRect);
    // setNoDecoration not called updateTitleBar when type is SplashScreen
    updateTitleBar();
    if (m_decoration) {
        if (hideDecoration)
            m_decoration->setVisible(false);
        m_decoration->stackBefore(m_surfaceItem);
    }

    m_surfaceItem->setVisible(true);
    Q_ASSERT(m_prelaunchSplash);
    m_prelaunchSplash->setVisible(false);
    m_prelaunchSplash->deleteLater();
    m_prelaunchSplash = nullptr;
    Q_EMIT prelaunchSplashChanged();

    // Now that the splash is hidden and deleted, the surface can be considered active if it's
    // mapped
    updateHasActiveCapability(ActiveControlState::MappedOrSplash, surface() && surface()->mapped());
    tryApplyDeferredSurfaceState();
}

WSurface *SurfaceWrapper::surface() const
{
    if (!m_shellSurface)
        return nullptr;
    auto *surf = m_shellSurface->surface();
    return surf;
}

WToplevelSurface *SurfaceWrapper::shellSurface() const
{
    return m_shellSurface;
}

WSurfaceItem *SurfaceWrapper::surfaceItem() const
{
    return m_surfaceItem;
}

QQuickItem *SurfaceWrapper::prelaunchSplash() const
{
    return m_prelaunchSplash;
}

QString SurfaceWrapper::appId() const
{
    // TODO: Need to consider `engineName` to ensure the uniqueness of appid.
    if (!m_appId.isEmpty())
        return m_appId;
    // TODO: consider to use "xdg-shell/" + m_shellSurface->appId(), need dde-shell adaptation
    if (m_shellSurface)
        return m_shellSurface->appId();
    return QString();
}

bool SurfaceWrapper::resize(const QSizeF &size, bool tryExec)
{
    // No surfaceItem in prelaunch mode -> return false
    if (!m_surfaceItem)
        return false;

    return m_surfaceItem->resizeSurface(size, tryExec);
}

void SurfaceWrapper::close()
{
    if (m_type == Type::SplashScreen) {
        // For splash screens, emit a signal to request closure
        Q_EMIT splashCloseRequested();
    } else if (m_shellSurface) {
        // For normal surfaces, call the shell surface's close method
        m_shellSurface->close();
    }
}

QRectF SurfaceWrapper::titlebarGeometry() const
{
    return m_titleBar ? QRectF({ 0, 0 }, m_titleBar->size()) : QRectF();
}

QRectF SurfaceWrapper::boundingRect() const
{
    return m_boundedRect;
}

QRectF SurfaceWrapper::normalGeometry() const
{
    return m_normalGeometry;
}

bool SurfaceWrapper::hasReliableNormalGeometry() const
{
    return m_normalGeometrySource == NormalGeometrySource::Client;
}

void SurfaceWrapper::moveNormalGeometryInOutput(const QPointF &position)
{
    QPointF alignedPosition = alignToPixelGrid(position);
    setNormalGeometry(QRectF(alignedPosition, m_normalGeometry.size()));
    if (isNormal()) {
        setPosition(alignedPosition);
    } else if (m_pendingState == State::Normal && m_geometryAnimation) {
        m_geometryAnimation->setProperty("toGeometry", m_normalGeometry);
    }
}

void SurfaceWrapper::setNormalGeometry(const QRectF &newNormalGeometry, bool applyDeferredState)
{
    if (m_normalGeometry == newNormalGeometry) {
        if (applyDeferredState)
            tryApplyDeferredSurfaceState();
        return;
    }
    m_normalGeometry = newNormalGeometry;
    Q_EMIT normalGeometryChanged();
    if (applyDeferredState)
        tryApplyDeferredSurfaceState();
}

void SurfaceWrapper::setNormalGeometryFromSurface(const QRectF &newNormalGeometry,
                                                  bool applyDeferredState)
{
    m_normalGeometrySource = NormalGeometrySource::Client;
    setNormalGeometry(newNormalGeometry, false);
    if (applyDeferredState)
        tryApplyDeferredSurfaceState();
}

void SurfaceWrapper::setRestoredNormalSize(const QSizeF &size, bool applyInitialMaximize)
{
    if (!size.isValid() || size.isEmpty()
        || m_normalGeometrySource == NormalGeometrySource::Client) {
        return;
    }

    const QPointF topLeft = m_normalGeometrySource == NormalGeometrySource::None
        ? position()
        : m_normalGeometry.topLeft();
    m_normalGeometrySource = NormalGeometrySource::Restored;
    setNormalGeometry(QRectF(topLeft, size), false);
    if (applyInitialMaximize)
        tryApplyInitialMaximize();
    tryApplyDeferredSurfaceState();
}

bool SurfaceWrapper::hasUsableNormalGeometry() const
{
    return m_normalGeometrySource != NormalGeometrySource::None && m_normalGeometry.isValid()
        && !m_normalGeometry.isEmpty();
}

bool SurfaceWrapper::captureNormalGeometryFromSurfaceItem(bool applyDeferredState)
{
    if (!m_surfaceItem || !m_surfaceItem->isReady()
        || (m_type != Type::XdgToplevel && m_type != Type::XWayland)) {
        return false;
    }

    const QSizeF surfaceSize(m_surfaceItem->implicitWidth(), m_surfaceItem->implicitHeight());
    if (surfaceSize.isEmpty() || !surfaceSize.isValid())
        return false;

    QRectF clientNormalGeometry = geometry();
    if (m_prelaunchSplash) {
        QPointF topLeft = m_normalGeometry.isValid() ? m_normalGeometry.topLeft() : position();
        if (m_type != Type::XWayland) {
            topLeft = geometry().center()
                - QPointF(surfaceSize.width() / 2.0, surfaceSize.height() / 2.0);
        }
        clientNormalGeometry = QRectF(topLeft, surfaceSize);
    } else if (!clientNormalGeometry.isValid() || clientNormalGeometry.isEmpty()) {
        clientNormalGeometry = QRectF(position(), surfaceSize);
    }

    setNormalGeometryFromSurface(clientNormalGeometry, applyDeferredState);
    qCDebug(lcTlSurface) << "Captured client normal geometry for" << appId()
                         << clientNormalGeometry << "prelaunch" << bool(m_prelaunchSplash);
    return true;
}

QRectF SurfaceWrapper::maximizedGeometry() const
{
    return m_maximizedGeometry;
}

void SurfaceWrapper::setMaximizedGeometry(const QRectF &newMaximizedGeometry)
{
    if (m_wrapperAboutToRemove)
        return;
    if (m_maximizedGeometry == newMaximizedGeometry)
        return;
    m_maximizedGeometry = newMaximizedGeometry;
    // This geometry change might be caused by a change in the output size due to screen scaling.
    // Ensure that the surfaceSizeRatio is updated before modifying the window size
    // to avoid incorrect sizing of Xwayland windows.
    updateSurfaceSizeRatio();

    if (m_pendingState == State::Maximized && m_geometryAnimation) {
        m_pendingGeometry = newMaximizedGeometry;
        m_geometryAnimation->setProperty("toGeometry", newMaximizedGeometry);
    }
    if (hasConfiguredInitialXdgMaximize()) {
        m_initialMaximizeGeometry =
            QRectF(alignToPixelGrid(newMaximizedGeometry.topLeft()),
                   newMaximizedGeometry.size());
        refreshConfiguredInitialXdgMaximize();
        tryApplyInitialMaximize();
    } else if (m_initialMaximizePending) {
        if (m_initialMaximizeConfigured) {
            m_initialMaximizeConfigured = false;
            ++m_initialMaximizeGeneration;
        }
        tryApplyInitialMaximize();
    } else if (m_surfaceState == State::Maximized) {
        setPosition(newMaximizedGeometry.topLeft());
        resize(newMaximizedGeometry.size());
    }

    Q_EMIT maximizedGeometryChanged();
    tryApplyDeferredSurfaceState();
}

QRectF SurfaceWrapper::fullscreenGeometry() const
{
    return m_fullscreenGeometry;
}

void SurfaceWrapper::setFullscreenGeometry(const QRectF &newFullscreenGeometry)
{
    if (m_wrapperAboutToRemove)
        return;
    if (m_fullscreenGeometry == newFullscreenGeometry)
        return;
    m_fullscreenGeometry = newFullscreenGeometry;
    // This geometry change might be caused by a change in the output size due to screen scaling.
    // Ensure that the surfaceSizeRatio is updated before modifying the window size
    // to avoid incorrect sizing of Xwayland windows.
    updateSurfaceSizeRatio();

    if (m_pendingState == State::Fullscreen && m_geometryAnimation) {
        m_pendingGeometry = newFullscreenGeometry;
        m_geometryAnimation->setProperty("toGeometry", newFullscreenGeometry);
    }
    if (m_surfaceState == State::Fullscreen) {
        setPosition(newFullscreenGeometry.topLeft());
        resize(newFullscreenGeometry.size());
    }

    Q_EMIT fullscreenGeometryChanged();

    updateClipRect();
    tryApplyDeferredSurfaceState();
}

QRectF SurfaceWrapper::tilingGeometry() const
{
    return m_tilingGeometry;
}

void SurfaceWrapper::setTilingGeometry(const QRectF &newTilingGeometry)
{
    if (m_wrapperAboutToRemove)
        return;
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

    Q_EMIT tilingGeometryChanged();
    tryApplyDeferredSurfaceState();
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
    Q_EMIT positionAutomaticChanged();
}

void SurfaceWrapper::resetWidth()
{
    if (m_surfaceItem)
        m_surfaceItem->resetWidth();
    QQuickItem::resetWidth();
}

void SurfaceWrapper::resetHeight()
{
    if (m_surfaceItem)
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
    if (m_wrapperAboutToRemove)
        return;

    if (m_ownsOutput == newOwnsOutput)
        return;

    if (m_ownsOutput) {
        m_ownsOutput->removeSurface(this);
    }

    m_ownsOutput = newOwnsOutput;

    if (m_ownsOutput) {
        m_ownsOutput->addSurface(this);
    }

    Q_EMIT ownsOutputChanged();
}

void SurfaceWrapper::setOutputs(const QList<WOutput *> &outputs)
{
    if (m_wrapperAboutToRemove)
        return;
    if (m_type == Type::SplashScreen) {
        m_prelaunchOutputs = outputs;
        return;
    }
    if (!surface()) {
        qCDebug(lcTlSurface) << "SurfaceWrapper::setOutputs called but surface() is null!";
        return;
    }
    const auto oldOutputs = surface()->outputs();
    for (auto output : std::as_const(oldOutputs)) {
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

const QList<WOutput *> &SurfaceWrapper::outputs() const
{
    if (m_type == Type::SplashScreen) {
        return m_prelaunchOutputs;
    }
    if (!surface()) {
        static const QList<WOutput *> empty;
        return empty;
    }
    return surface()->outputs();
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
    if (m_wrapperAboutToRemove)
        return;

    if (m_initialMaximizePending) {
        if (newSurfaceState == State::Maximized) {
            tryApplyInitialMaximize();
        } else {
            deferSurfaceState(newSurfaceState);
        }
        return;
    }

    if (m_geometryAnimation) {
        deferSurfaceState(newSurfaceState);
        return;
    }

    if (m_windowAnimation) {
        deferSurfaceState(newSurfaceState);
        return;
    }

    if (m_surfaceState == newSurfaceState) {
        m_hasDeferredSurfaceState = false;
        return;
    }

    if (!hasInitializeContainer() || !m_surfaceItem) {
        deferSurfaceState(newSurfaceState);
        return;
    }

    if (container()->filterSurfaceStateChange(this, newSurfaceState, m_surfaceState))
        return;

    if (newSurfaceState == State::Minimized) {
        doSetSurfaceState(newSurfaceState);
        return;
    }

    const bool isManagedToplevel =
        m_type == Type::XdgToplevel || m_type == Type::XWayland;
    const bool waitingForSurfaceGeometry =
        isManagedToplevel && (!m_surfaceItem->isReady() || !geometry().isValid());
    const bool waitingForMappedXwayland =
        m_type == Type::XWayland
        && (!surface() || !surface()->mapped() || !isVisible());
    if (waitingForSurfaceGeometry || waitingForMappedXwayland) {
        deferSurfaceState(newSurfaceState);
        return;
    }

    if (isManagedToplevel && m_surfaceState == State::Normal
        && newSurfaceState != State::Normal
        && !captureNormalGeometryFromSurfaceItem()) {
        deferSurfaceState(newSurfaceState);
        return;
    }

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

    if (!targetGeometry.isValid()
        || (isManagedToplevel && newSurfaceState == State::Normal
            && !hasUsableNormalGeometry())) {
        deferSurfaceState(newSurfaceState);
        return;
    }

    startStateChangeAnimation(newSurfaceState, targetGeometry);
}

void SurfaceWrapper::deferSurfaceState(State newSurfaceState)
{
    m_deferredSurfaceState = newSurfaceState;
    m_hasDeferredSurfaceState = true;
}

void SurfaceWrapper::tryApplyDeferredSurfaceState()
{
    if (!m_hasDeferredSurfaceState || m_geometryAnimation || m_initialMaximizePending
        || m_wrapperAboutToRemove) {
        return;
    }

    const State deferredState = m_deferredSurfaceState;
    m_hasDeferredSurfaceState = false;
    setSurfaceState(deferredState);
}

void SurfaceWrapper::tryApplyInitialMaximize()
{
    if (m_type == Type::XdgToplevel) {
        if (!hasConfiguredInitialXdgMaximize() || m_wrapperAboutToRemove || m_windowAnimation
            || m_geometryAnimation || !hasInitializeContainer() || !m_surfaceItem
            || !m_surfaceItem->isReady() || !surface() || !surface()->mapped()) {
            return;
        }

        armInitialMaximizeCommitTimeout();
        handleInitialMaximizeCommit();
        return;
    }

    if (!m_initialMaximizePending || m_initialMaximizeConfigured || m_wrapperAboutToRemove
        || m_windowAnimation || m_geometryAnimation || !hasInitializeContainer() || !m_surfaceItem
        || !m_surfaceItem->isReady() || !surface() || !surface()->mapped()
        || !m_maximizedGeometry.isValid() || m_maximizedGeometry.isEmpty()) {
        return;
    }

    const QRectF targetGeometry(alignToPixelGrid(m_maximizedGeometry.topLeft()),
                                m_maximizedGeometry.size());
    const QSizeF surfaceSize(m_surfaceItem->implicitWidth(), m_surfaceItem->implicitHeight());
    const bool surfaceAlreadyMaximized = sizesMatch(surfaceSize, targetGeometry.size());

    // A target-sized first buffer is presentation state, not evidence of the client's normal
    // bounds. Preserve a restored prelaunch/DConfig size in that case.
    if (!hasReliableNormalGeometry() && !surfaceAlreadyMaximized)
        captureNormalGeometryFromSurfaceItem(false);

    if (!hasUsableNormalGeometry()) {
        const QSizeF fallbackSize(qMin<qreal>(defaultNormalWidth, targetGeometry.width()),
                                  qMin<qreal>(defaultNormalHeight, targetGeometry.height()));
        setRestoredNormalSize(fallbackSize, false);
        qCDebug(lcTlSurface) << "Using non-persisted normal geometry fallback for" << appId()
                             << fallbackSize << "before initial maximize";
    }
    if (!hasUsableNormalGeometry())
        return;

    // Keep the last normal buffer available for capture, then hide the real item before the
    // state configure is sent. Presentation resumes only after the matching buffer commit.
    m_surfaceItem->setVisible(false);
    bool configured = false;
    setXwaylandPositionFromSurface(false);
    if (m_type == Type::XWayland) {
        auto *xwaylandItem = qobject_cast<WXWaylandSurfaceItem *>(m_surfaceItem);
        configured =
            xwaylandItem && xwaylandItem->configureSurfaceWhileHidden(targetGeometry);
    } else {
        configured = resize(targetGeometry.size());
    }

    if (!configured) {
        setXwaylandPositionFromSurface(true);
        qCDebug(lcTlSurface) << "Initial maximize configure is not ready for" << appId()
                             << "target" << targetGeometry;
        return;
    }

    m_initialMaximizeGeometry = targetGeometry;
    m_initialMaximizeConfigured = true;
    if (m_surfaceState != State::Maximized)
        doSetSurfaceState(State::Maximized);

    qCDebug(lcTlSurface) << "Configured hidden initial maximize for" << appId() << "type"
                         << static_cast<int>(m_type) << "target" << targetGeometry
                         << "normal" << m_normalGeometry << "normalFromClient"
                         << hasReliableNormalGeometry() << "prelaunch"
                         << bool(m_prelaunchSplash);

    armInitialMaximizeCommitTimeout();
}

bool SurfaceWrapper::hasConfiguredInitialXdgMaximize() const
{
    return m_type == Type::XdgToplevel && m_initialMaximizePending
        && m_initialMaximizeConfigured && m_initialMaximizeGeometry.isValid()
        && !m_initialMaximizeGeometry.isEmpty();
}

void SurfaceWrapper::refreshConfiguredInitialXdgMaximize()
{
    if (!hasConfiguredInitialXdgMaximize() || !m_shellSurface
        || !m_shellSurface->isInitialized()) {
        return;
    }

    if (resize(m_initialMaximizeGeometry.size()))
        m_shellSurface->setMaximize(true);
}

void SurfaceWrapper::activateConfiguredInitialXdgMaximize()
{
    if (!hasConfiguredInitialXdgMaximize() || !m_surfaceItem || !isMaximizable())
        return;

    const QRectF targetGeometry(alignToPixelGrid(m_initialMaximizeGeometry.topLeft()),
                                m_initialMaximizeGeometry.size());
    m_initialMaximizeGeometry = targetGeometry;

    if (!hasUsableNormalGeometry()) {
        const QSizeF fallbackSize(qMin<qreal>(defaultNormalWidth, targetGeometry.width()),
                                  qMin<qreal>(defaultNormalHeight, targetGeometry.height()));
        setRestoredNormalSize(fallbackSize, false);
        qCDebug(lcTlSurface) << "Using non-persisted normal geometry fallback for" << appId()
                             << fallbackSize << "before initial XDG maximize";
    }

    m_surfaceItem->setVisible(false);
    setPosition(targetGeometry.topLeft());
    if (m_surfaceState != State::Maximized)
        doSetSurfaceState(State::Maximized, false);

    qCDebug(lcTlSurface) << "Adopted configured initial XDG maximize for" << appId() << "target"
                         << targetGeometry << "normal" << m_normalGeometry << "prelaunch"
                         << bool(m_prelaunchSplash);
}

void SurfaceWrapper::cancelConfiguredInitialXdgMaximize()
{
    if (!hasConfiguredInitialXdgMaximize())
        return;

    m_initialMaximizePending = false;
    m_initialMaximizeConfigured = false;
    ++m_initialMaximizeGeneration;

    if (hasUsableNormalGeometry())
        resize(m_normalGeometry.size());
    else
        m_shellSurface->resize(QSize());

    if (m_surfaceState == State::Maximized)
        doSetSurfaceState(State::Normal);
    else
        m_shellSurface->setMaximize(false);

    if (m_prelaunchSplash) {
        if (surface() && surface()->mapped())
            startPrelaunchSplashHideSequence();
    } else {
        m_surfaceItem->setVisible(true);
        if (surface() && surface()->mapped() && !m_windowAnimation)
            createNewOrClose(OPEN_ANIMATION);
    }

    qCDebug(lcTlSurface) << "Cancelled configured initial XDG maximize for" << appId();
    tryApplyDeferredSurfaceState();
}

void SurfaceWrapper::armInitialMaximizeCommitTimeout()
{
    const quint64 generation = ++m_initialMaximizeGeneration;
    QTimer::singleShot(initialMaximizeCommitTimeoutMs, this, [this, generation] {
        if (!m_initialMaximizePending || !m_initialMaximizeConfigured
            || generation != m_initialMaximizeGeneration) {
            return;
        }

        if (initialMaximizeTargetCommitted()) {
            finishInitialMaximize(false);
            return;
        }

        qCWarning(lcTlSurface) << "Timed out waiting for initial maximized buffer from" << appId()
                               << "target" << m_initialMaximizeGeometry << "actual"
                               << QSizeF(m_surfaceItem->implicitWidth(),
                                         m_surfaceItem->implicitHeight());
        finishInitialMaximize(true);
    });
}

void SurfaceWrapper::handleInitialMaximizeCommit()
{
    if (!m_initialMaximizePending || !m_initialMaximizeConfigured || m_wrapperAboutToRemove
        || !surface() || !surface()->mapped()) {
        return;
    }

    if (!initialMaximizeTargetCommitted()) {
        qCDebug(lcTlSurface) << "Waiting for initial maximized buffer from" << appId()
                             << "target" << m_initialMaximizeGeometry.size() << "actual"
                             << QSizeF(m_surfaceItem->implicitWidth(),
                                       m_surfaceItem->implicitHeight());
        return;
    }

    finishInitialMaximize(false);
}

bool SurfaceWrapper::initialMaximizeTargetCommitted() const
{
    if (!m_surfaceItem || !m_initialMaximizeGeometry.isValid())
        return false;

    const QSizeF actualSize(m_surfaceItem->implicitWidth(), m_surfaceItem->implicitHeight());
    const QSizeF targetSize = m_initialMaximizeGeometry.size();
    return sizesMatch(actualSize, targetSize);
}

void SurfaceWrapper::finishInitialMaximize(bool timedOut)
{
    if (!m_initialMaximizePending)
        return;

    const QRectF targetGeometry = m_initialMaximizeGeometry;
    m_initialMaximizePending = false;
    m_initialMaximizeConfigured = false;
    ++m_initialMaximizeGeneration;

    setPosition(targetGeometry.topLeft());
    setImplicitSize(targetGeometry.width(), targetGeometry.height());
    setXwaylandPositionFromSurface(true);

    if (m_prelaunchSplash) {
        completeSplashTransition(targetGeometry.size());
    } else {
        m_surfaceItem->setVisible(true);
        updateBoundingRect();
        if (surface() && surface()->mapped() && !m_windowAnimation)
            createNewOrClose(OPEN_ANIMATION);
    }

    qCDebug(lcTlSurface) << "Presented initial maximized surface for" << appId() << "target"
                         << targetGeometry << "timedOut" << timedOut;
    tryApplyDeferredSurfaceState();
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

bool SurfaceWrapper::isWindowAnimationRunning() const
{
    return !m_windowAnimation.isNull();
}

void SurfaceWrapper::destroy()
{
    invalidate();
    if (!isWindowAnimationRunning())
        deleteLater();
    // else delete this in Animation(for window close animation) finish
}

bool SurfaceWrapper::acceptKeyboardFocus() const
{
    return m_focusControlStates.testFlag(FocusControlState::AcceptKeyboardFocus);
}

void SurfaceWrapper::setAcceptKeyboardFocus(bool accept)
{
    if (acceptKeyboardFocus() == accept)
        return;

    updateFocusControlState(FocusControlState::AcceptKeyboardFocus, accept);
}

bool SurfaceWrapper::isActivated() const
{
    return m_isActivated;
}

bool SurfaceWrapper::attention() const
{
    return m_attention;
}

bool SurfaceWrapper::setAttention(bool attention)
{
    if (m_attention == attention)
        return true;
    if (attention && m_isActivated) {
        qCWarning(lcTlSurface) << "setAttention(true) ignored: surface is already activated";
        return false;
    }
    m_attention = attention;
    Q_EMIT attentionChanged();
    return true;
}

bool SurfaceWrapper::isInputPopupLike() const
{
    return m_type == Type::InputPopup || m_isIMCandidatePanel;
}

bool SurfaceWrapper::isIMCandidatePanel() const
{
    return m_isIMCandidatePanel;
}

void SurfaceWrapper::setIMCandidatePanel(bool isIMCandidatePanel)
{
    if (m_isIMCandidatePanel == isIMCandidatePanel)
        return;
    m_isIMCandidatePanel = isIMCandidatePanel;
    Q_EMIT isIMCandidatePanelChanged();
}

void SurfaceWrapper::setNoDecoration(bool newNoDecoration)
{
    if (m_wrapperAboutToRemove)
        return;

    if (m_noDecoration == newNoDecoration)
        return;

    m_noDecoration = newNoDecoration;
    setNoCornerRadius(newNoDecoration);

    updateDecoration();
    refreshConfiguredInitialXdgMaximize();
}

void SurfaceWrapper::updateDecoration()
{
    if (m_type != Type::SplashScreen)
        updateTitleBar();

    if (m_noDecoration) {
        Q_ASSERT(m_decoration);
        m_decoration->disconnect(this);
        m_decoration->deleteLater();
        m_decoration = nullptr;
    } else {
        Q_ASSERT(!m_decoration);
        Q_ASSERT(m_surfaceItem || m_prelaunchSplash);
        m_decoration = m_engine->createDecoration(this, this);
        m_decoration->stackBefore(m_surfaceItem ? m_surfaceItem : m_prelaunchSplash);
        connect(m_decoration, &QQuickItem::xChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration, &QQuickItem::yChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration, &QQuickItem::widthChanged, this, &SurfaceWrapper::updateBoundingRect);
        connect(m_decoration,
                &QQuickItem::heightChanged,
                this,
                &SurfaceWrapper::updateBoundingRect);
    }

    updateBoundingRect();
    Q_EMIT noDecorationChanged();
}

void SurfaceWrapper::updateTitleBar()
{
    if (m_wrapperAboutToRemove)
        return;

    // No surfaceItem in prelaunch mode -> early return
    if (!m_surfaceItem)
        return;

    if (noTitleBar() == !m_titleBar)
        return;

    if (m_titleBar) {
        m_titleBar->disconnect(this);
        m_titleBar->deleteLater();
        m_titleBar = nullptr;
        m_surfaceItem->setTopPadding(0);
    } else {
        m_titleBar = m_engine->createTitleBar(this, m_surfaceItem);
        m_titleBar->setZ(static_cast<int>(WSurfaceItem::ZOrder::ContentItem));
        m_surfaceItem->setTopPadding(m_titleBar->height());
        connect(m_titleBar, &QQuickItem::heightChanged, this, [this] {
            m_surfaceItem->setTopPadding(m_titleBar->height());
        });
    }

    Q_EMIT noTitleBarChanged();
}

void SurfaceWrapper::setBoundedRect(const QRectF &newBoundedRect)
{
    if (m_boundedRect == newBoundedRect)
        return;
    m_boundedRect = newBoundedRect;
    Q_EMIT boundingRectChanged();
}

void SurfaceWrapper::updateBoundingRect()
{
    QRectF rect(QRectF(QPointF(0, 0), size()));
    if (m_surfaceItem)
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
    if (m_wrapperAboutToRemove)
        return;
    bool isVisible = !m_hideByWorkspace && !isMinimized()
        && ((surface() && surface()->mapped())
            || (m_prelaunchSplash && m_prelaunchSplash->isVisible()))
        && m_socketEnabled && m_hideByshowDesk && !m_confirmHideByLockScreen
        && Helper::instance()->surfaceBelongsToCurrentSession(this);
    setVisible(isVisible);
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
    auto rw = qobject_cast<WOutputRenderWindow *>(window());
    Q_ASSERT(rw);
    rw->markItemClipRectDirty(this);
}

void SurfaceWrapper::geometryChange(const QRectF &newGeo, const QRectF &oldGeometry)
{
    QRectF newGeometry = newGeo;
    if (m_container && m_container->filterSurfaceGeometryChanged(this, newGeometry, oldGeometry))
        return;

    if (isNormal() && !m_geometryAnimation && !m_initialMaximizePending) {
        const bool comesFromSurface =
            m_shellSurface && !m_prelaunchSplash && m_surfaceItem && m_surfaceItem->isReady()
            && (m_type == Type::XdgToplevel || m_type == Type::XWayland);
        if (comesFromSurface)
            setNormalGeometryFromSurface(newGeometry, false);
        else
            setNormalGeometry(newGeometry, false);
    }

    if (widthValid() && heightValid()) {
        resize(newGeometry.size());
    }

    Q_EMIT geometryChanged();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        updateBoundingRect();
    updateClipRect();
    tryApplyDeferredSurfaceState();
}

void SurfaceWrapper::createNewOrClose(uint direction)
{
    if (!m_windowAnimationEnabled)
        return;

    if (m_windowAnimation)
        return;

    if (m_container.isNull())
        return;

    switch (m_type) {
    case Type::SplashScreen:
        [[fallthrough]];
    case Type::XdgToplevel:
        [[fallthrough]];
    case Type::XWayland: {
        m_windowAnimation = m_engine->createNewAnimation(this, container(), direction);
        m_windowAnimation->setProperty("enableBlur", m_blur);
    } break;
    case Type::Layer: {
        auto scope = QString(static_cast<WLayerSurfaceItem *>(m_surfaceItem)
                                 ->layerSurface()
                                 ->handle()
                                 ->handle()
                                 ->scope);
        auto *surface = qobject_cast<WLayerSurface *>(m_shellSurface);
        auto anchor = surface->getExclusiveZoneEdge();
        if (scope == "dde-shell/launchpad") {
            m_windowAnimation = m_engine->createLaunchpadAnimation(this, direction, m_container);
        } else if (anchor != WLayerSurface::AnchorType::None) {
            m_windowAnimation = m_engine->createLayerShellAnimation(this, container(), direction);
            m_windowAnimation->setProperty("position", QVariant::fromValue(anchor));
            m_windowAnimation->setProperty("enableBlur", m_blur);
        } else {
            // NOTE: missing fullscreen window animation, so hide window now.
            if (m_hideByLockScreen) {
                m_confirmHideByLockScreen = true;
                updateVisible();
            }
        }
    }; break;
    case Type::XdgPopup:
        // NOTE: check z order for XdgToplevel parent/child after support popup animation
        [[fallthrough]];
    default:
        break;
    }

    if (m_windowAnimation) {
        if (Helper::instance()->noAnimation()) {
            if (direction == OPEN_ANIMATION) {
                onShowAnimationFinished();
            } else {
                onHideAnimationFinished();
            }
            return;
        }
        bool ok = false;
        if (direction == OPEN_ANIMATION) {
            ok = connect(m_windowAnimation,
                         SIGNAL(finished()),
                         this,
                         SLOT(onShowAnimationFinished()));
        } else {
            ok = connect(m_windowAnimation,
                         SIGNAL(finished()),
                         this,
                         SLOT(onHideAnimationFinished()));
        }
        Q_ASSERT(ok);
        ok = QMetaObject::invokeMethod(m_windowAnimation, "start");
        Q_ASSERT(ok);

        Q_EMIT windowAnimationRunningChanged();
    }
}

void SurfaceWrapper::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemSceneChange) {
        updateSurfaceSizeRatio();
    }

    return QQuickItem::itemChange(change, data);
}

void SurfaceWrapper::doSetSurfaceState(State newSurfaceState, bool configureShellSurface)
{
    if (m_wrapperAboutToRemove)
        return;

    // In prelaunch mode there is no shellSurface; update state only without calling shellSurface
    // methods
    if (!m_shellSurface) {
        m_previousSurfaceState.setValueBypassingBindings(m_surfaceState);
        m_surfaceState.setValueBypassingBindings(newSurfaceState);
        m_previousSurfaceState.notify();
        m_surfaceState.notify();
        return;
    }

    const bool wasMinimized = (m_surfaceState == State::Minimized);
    const bool willBeMinimized = (newSurfaceState == State::Minimized);
    const bool needMinimizeLinkage = (wasMinimized != willBeMinimized);

    setVisibleDecoration(newSurfaceState == State::Minimized || newSurfaceState == State::Normal);
    setNoCornerRadius(newSurfaceState == State::Maximized || newSurfaceState == State::Fullscreen
                      || newSurfaceState == State::Tiling);

    m_previousSurfaceState.setValueBypassingBindings(m_surfaceState);
    m_surfaceState.setValueBypassingBindings(newSurfaceState);

    // Keep modal/parent minimize linkage ahead of this surface's own state change
    // so focus fallback never sees the parent in the old state first.
    if (needMinimizeLinkage && modal() && m_parentSurface) {
        if (willBeMinimized && !m_parentSurface->isMinimized()) {
            m_parentSurface->minimize(false);
        } else if (!willBeMinimized && m_parentSurface->isMinimized()) {
            m_parentSurface->restoreFromMinimized(false);
        }
    }

    if (configureShellSurface) {
        switch (m_previousSurfaceState.value()) {
        case State::Maximized:
            m_shellSurface->setMaximize(false);
            break;
        case State::Minimized:
            m_shellSurface->setMinimize(false);
            updateFocusControlState(FocusControlState::UnMinimized, true);
            updateHasActiveCapability(ActiveControlState::UnMinimized, true);
            break;
        case State::Fullscreen:
            m_shellSurface->setFullScreen(false);
            break;
        case State::Normal:
            [[fallthrough]];
        case State::Tiling:
            [[fallthrough]];
        default:
            break;
        }
    }
    m_previousSurfaceState.notify();

    if (configureShellSurface) {
        switch (m_surfaceState.value()) {
        case State::Maximized:
            m_shellSurface->setMaximize(true);
            break;
        case State::Minimized:
            updateFocusControlState(FocusControlState::UnMinimized, false);
            updateHasActiveCapability(ActiveControlState::UnMinimized, false);
            m_shellSurface->setMinimize(true);
            break;
        case State::Fullscreen:
            m_shellSurface->setFullScreen(true);
            break;
        case State::Normal:
            [[fallthrough]];
        case State::Tiling:
            [[fallthrough]];
        default:
            break;
        }
    }
    m_surfaceState.notify();
    updateTitleBar();
    updateVisible();

    if (needMinimizeLinkage) {
        for (SurfaceWrapper *child : std::as_const(m_subSurfaces)) {
            if (willBeMinimized && child->modal())
                continue; // Modal children stay visible when parent is minimized.
            if (child->isMinimized() != willBeMinimized) {
                if (willBeMinimized)
                    child->minimize(false);
                else
                    child->restoreFromMinimized(false);
            }
        }
    }
}

void SurfaceWrapper::onAnimationReady()
{
    Q_ASSERT(m_pendingState != m_surfaceState);
    Q_ASSERT(m_pendingGeometry.isValid());

    auto deferPendingState = [this] {
        const State deferredState = m_pendingState;
        setXwaylandPositionFromSurface(true);
        const QPointer<QQuickItem> failedAnimation = m_geometryAnimation;
        m_geometryAnimation->disconnect(this);
        m_geometryAnimation->deleteLater();
        m_geometryAnimation = nullptr;
        deferSurfaceState(deferredState);
        continuePendingTransitionsAfterAnimation(failedAnimation);
    };

    if (!resize(m_pendingGeometry.size(), true)) {
        // abort change state if cannot resize
        deferPendingState();
        return;
    }

    const bool completingXwaylandPrelaunch =
        m_type == Type::XWayland && m_prelaunchSplash;
    if (completingXwaylandPrelaunch) {
        // WXWaylandSurfaceItem intentionally ignores configure requests while it is hidden.
        // GeometryAnimation.hideSource already owns presentation at this point, so making the
        // real item visible enables the native configure without exposing it directly.
        if (!isVisible()) {
            deferPendingState();
            return;
        }
        m_surfaceItem->setVisible(true);
        Q_ASSERT(m_surfaceItem->isVisible());
    }

    if (m_type == Type::XWayland
        && (!surface() || !surface()->mapped() || !isVisible()
            || !m_surfaceItem->isVisible())) {
        deferPendingState();
        return;
    }

    QPointF alignedPos = alignToPixelGrid(m_pendingGeometry.topLeft());
    setPosition(alignedPos);
    bool resizeCallSucceeded = true;
    if (m_type == Type::XWayland) {
        // XWayland's configure path can reject a hidden item. Configure first so state is not
        // acknowledged unless the native geometry request was actually sent.
        resizeCallSucceeded = resize(m_pendingGeometry.size());
        if (!resizeCallSucceeded) {
            qCDebug(lcTlSurface) << "Deferred XWayland state because native configure was skipped"
                                 << appId() << m_pendingGeometry;
            deferPendingState();
            return;
        }
        doSetSurfaceState(m_pendingState);
    } else {
        doSetSurfaceState(m_pendingState);
        resizeCallSucceeded = resize(m_pendingGeometry.size());
    }

    if (m_type == Type::XWayland) {
        qCDebug(lcTlSurface) << "Requested XWayland state geometry for" << appId()
                             << "state" << static_cast<int>(m_pendingState)
                             << "target" << m_pendingGeometry
                             << "prelaunch" << completingXwaylandPrelaunch
                             << "surfaceItemVisible" << m_surfaceItem->isVisible()
                             << "resizeCallSucceeded" << resizeCallSucceeded;
    }

    if (m_prelaunchSplash && surface() && surface()->mapped())
        completeSplashTransition(m_pendingGeometry.size());
}

void SurfaceWrapper::onAnimationFinished()
{
    setXwaylandPositionFromSurface(true);
    Q_ASSERT(m_geometryAnimation);
    const QPointer<QQuickItem> finishedAnimation = m_geometryAnimation;
    m_geometryAnimation->disconnect(this);
    m_geometryAnimation->deleteLater();
    m_geometryAnimation = nullptr;

    continuePendingTransitionsAfterAnimation(finishedAnimation);
}

bool SurfaceWrapper::startStateChangeAnimation(State targetState, const QRectF &targetGeometry)
{
    if (m_geometryAnimation) // animation running
        return false;

    m_geometryAnimation =
        m_engine->createGeometryAnimation(this, geometry(), targetGeometry, container());
    m_geometryAnimation->setProperty("enableBlur", m_blur);
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

void SurfaceWrapper::continuePendingTransitionsAfterAnimation(QQuickItem *animation)
{
    Q_ASSERT(animation);
    connect(animation, &QObject::destroyed, this, [this] {
        // QObject::destroyed is emitted while the object is being torn down. Continue on the
        // next event-loop turn so every ShaderEffectSource child has released hideSource.
        QTimer::singleShot(0, this, [this] {
            if (m_wrapperAboutToRemove)
                return;

            tryApplyInitialMaximize();
            if (m_prelaunchSplash && surface() && surface()->mapped())
                startPrelaunchSplashHideSequence();
            tryApplyDeferredSurfaceState();
        });
    });
}

void SurfaceWrapper::onWindowAnimationFinished()
{
    Q_ASSERT(m_windowAnimation);
    m_windowAnimation->disconnect(this);
    m_windowAnimation->deleteLater();
    m_windowAnimation = nullptr;

    Q_EMIT windowAnimationRunningChanged();

    if (m_wrapperAboutToRemove) {
        deleteLater();
    }
}

void SurfaceWrapper::onShowAnimationFinished()
{
    Q_ASSERT(m_windowAnimation);
    const QPointer<QQuickItem> finishedAnimation = m_windowAnimation;
    onWindowAnimationFinished();

    // onWindowAnimationFinished() disconnects the animation before scheduling its deletion, so
    // install the destruction continuation afterwards.
    continuePendingTransitionsAfterAnimation(finishedAnimation);
}

void SurfaceWrapper::onHideAnimationFinished()
{
    if (m_coverContent) {
        m_coverContent->setVisible(false);
    }

    if (m_hideByLockScreen) {
        m_confirmHideByLockScreen = true;
        updateVisible();
    }

    onWindowAnimationFinished();
}

void SurfaceWrapper::onMappedChanged()
{
    if (m_wrapperAboutToRemove)
        return;

    Q_ASSERT(surface());
    bool mapped = surface()->mapped() && !m_hideByLockScreen;

    if (!m_isProxy) {
        if (mapped) {
            if (!m_prelaunchSplash) {
                if (m_initialMaximizePending) {
                    tryApplyInitialMaximize();
                } else if (!m_geometryAnimation) {
                    createNewOrClose(OPEN_ANIMATION);
                }
            } else {
                syncPrelaunchMappedState();
                startPrelaunchSplashHideSequence();
            }
            if (m_coverContent) {
                m_coverContent->setVisible(true);
            }
        } else {
            createNewOrClose(CLOSE_ANIMATION);
        }
    }

    if (m_coverContent) {
        m_coverContent->setProperty("mapped", mapped);
    }

    // Splash can't call onMappedChanged, just use mapped state to update
    updateHasActiveCapability(ActiveControlState::MappedOrSplash, mapped);
    updateFocusControlState(FocusControlState::Mapped, mapped);
    updateVisible();
    if (mapped) {
        tryApplyInitialMaximize();
        tryApplyDeferredSurfaceState();
    }
}

void SurfaceWrapper::onSocketEnabledChanged()
{
    if (auto client = shellSurface()->waylandClient()) {
        m_socketEnabled = client->socket()->rootSocket()->isEnabled();
        updateVisible();
    }
}

void SurfaceWrapper::onMinimizeAnimationFinished()
{
    Q_ASSERT(m_minimizeAnimation);
    m_minimizeAnimation->disconnect(this);
    m_minimizeAnimation->deleteLater();
    m_minimizeAnimation = nullptr;
}

void SurfaceWrapper::startMinimizeAnimation(const QRectF &iconGeometry, uint direction)
{
    if (m_minimizeAnimation)
        return;
    if (!Helper::instance()->surfaceBelongsToCurrentSession(this))
        return;

    m_minimizeAnimation =
        m_engine->createMinimizeAnimation(this, container(), iconGeometry, direction);

    bool ok =
        connect(m_minimizeAnimation, SIGNAL(finished()), this, SLOT(onMinimizeAnimationFinished()));
    Q_ASSERT(ok);
    ok = QMetaObject::invokeMethod(m_minimizeAnimation, "start");
    Q_ASSERT(ok);
}

void SurfaceWrapper::setHideByShowDesk(bool show)
{
    if (m_hideByshowDesk == show)
        return;

    m_hideByshowDesk = show;
}

void SurfaceWrapper::setHideByLockScreen(bool hide)
{
    if (m_hideByLockScreen == hide) {
        return;
    }

    m_hideByLockScreen = hide;
    m_confirmHideByLockScreen = false; // reset

    onMappedChanged();
}

void SurfaceWrapper::onShowDesktopAnimationFinished()
{
    Q_ASSERT(m_showDesktopAnimation);
    m_showDesktopAnimation->disconnect(this);
    m_showDesktopAnimation->deleteLater();
    m_showDesktopAnimation = nullptr;
    updateVisible();
}

void SurfaceWrapper::startShowDesktopAnimation(bool show)
{

    if (m_showDesktopAnimation) {
        m_showDesktopAnimation->disconnect(this);
        m_showDesktopAnimation->deleteLater();
        m_showDesktopAnimation = nullptr;
    }

    setHideByShowDesk(show);
    m_showDesktopAnimation = m_engine->createShowDesktopAnimation(this, container(), show);
    bool ok = connect(m_showDesktopAnimation,
                      SIGNAL(finished()),
                      this,
                      SLOT(onShowDesktopAnimationFinished()));
    Q_ASSERT(ok);
    ok = QMetaObject::invokeMethod(m_showDesktopAnimation, "start");
    Q_ASSERT(ok);
}

qreal SurfaceWrapper::radius() const
{
    if (isInputPopupLike())
        return 0;
    if (m_type == Type::XdgPopup)
        return 8;

    qreal radius = m_radius;
    // m_radius > 1 means radius was explicitly set via Personalization protocol;
    // m_radius <= 1 means no per-window override, fall back to DConfig.
    if (radius < 1 && m_type != Type::Layer) {
        radius = Helper::instance()->config()->windowRadius();
    }

    return radius;
}

void SurfaceWrapper::setRadius(qreal newRadius)
{
    if (qFuzzyCompare(m_radius, newRadius))
        return;
    m_radius = newRadius;
    Q_EMIT radiusChanged();
}

void SurfaceWrapper::minimize(bool onAnimation)
{
    if (m_surfaceState == State::Minimized)
        return;
    setSurfaceState(State::Minimized);
    if (onAnimation)
        startMinimizeAnimation(iconGeometry(), CLOSE_ANIMATION);
}

void SurfaceWrapper::restoreFromMinimized(bool onAnimation)
{
    if (m_surfaceState != State::Minimized && m_hideByshowDesk)
        return;
    if (!m_hideByshowDesk)
        setHideByShowDesk(true);

    doSetSurfaceState(m_previousSurfaceState);
    if (onAnimation)
        startMinimizeAnimation(iconGeometry(), OPEN_ANIMATION);
}

void SurfaceWrapper::maximize()
{
    if (m_surfaceState == State::Minimized || m_surfaceState == State::Fullscreen
        || !isMaximizable())
        return;

    // ShellHandler folds a native XDG launch-time request into the initial configure. Calling
    // set_maximized before the initial empty commit would violate wlroots' initialized
    // precondition, while waiting for map would turn it into a second visible state change.
    if (m_type == Type::XdgToplevel && (!surface() || !surface()->mapped()))
        return;

    const bool prelaunchGeometryTransition =
        m_geometryAnimation && m_pendingState == m_surfaceState;
    const bool isLaunchTimeRequest =
        m_type == Type::XWayland
        && (!hasInitializeContainer() || m_prelaunchSplash || m_windowAnimation
            || prelaunchGeometryTransition || !m_surfaceItem || !m_surfaceItem->isReady()
            || !surface() || !surface()->mapped());
    if (!m_initialMaximizePending && isLaunchTimeRequest) {
        m_initialMaximizePending = true;
        // A running NewAnimation uses a live ShaderEffectSource. Keep its source intact until
        // the animation object is gone; tryApplyInitialMaximize() hides it immediately after.
        if (m_surfaceItem && (!m_windowAnimation || m_prelaunchSplash))
            m_surfaceItem->setVisible(false);
        qCDebug(lcTlSurface) << "Promoted launch-time maximize request for" << appId() << "type"
                             << static_cast<int>(m_type);
        tryApplyInitialMaximize();
        return;
    }

    setSurfaceState(State::Maximized);
}

void SurfaceWrapper::unmaximize()
{
    if (hasConfiguredInitialXdgMaximize()) {
        cancelConfiguredInitialXdgMaximize();
        return;
    }

    if (m_initialMaximizePending) {
        if (!m_initialMaximizeConfigured && m_surfaceState == State::Normal) {
            m_initialMaximizePending = false;
            ++m_initialMaximizeGeneration;
            setXwaylandPositionFromSurface(true);

            if (m_prelaunchSplash && surface() && surface()->mapped()) {
                startPrelaunchSplashHideSequence();
            } else if (!m_prelaunchSplash && !m_windowAnimation && !m_geometryAnimation) {
                m_surfaceItem->setVisible(true);
                if (surface() && surface()->mapped())
                    createNewOrClose(OPEN_ANIMATION);
            }

            qCDebug(lcTlSurface) << "Cancelled unconfigured initial maximize for" << appId();
            return;
        }

        setSurfaceState(State::Normal);
        return;
    }

    if (m_surfaceState != State::Maximized) {
        const bool maximizeAnimationPending =
            m_geometryAnimation && m_pendingState == State::Maximized;
        const bool maximizeStateDeferred =
            m_hasDeferredSurfaceState && m_deferredSurfaceState == State::Maximized;
        if (maximizeAnimationPending || maximizeStateDeferred)
            setSurfaceState(State::Normal);
        return;
    }

    setSurfaceState(State::Normal);
}

void SurfaceWrapper::toggleMaximized()
{
    if (m_surfaceState == State::Maximized)
        unmaximize();
    else
        maximize();
}

void SurfaceWrapper::enterFullscreen()
{
    if (m_surfaceState == State::Minimized)
        return;

    setSurfaceState(State::Fullscreen);
}

void SurfaceWrapper::leaveFullscreen()
{
    if (m_surfaceState != State::Fullscreen)
        return;

    setSurfaceState(m_previousSurfaceState);
}

void SurfaceWrapper::closeSurface()
{
    // No shellSurface in prelaunch mode -> early return
    if (!m_shellSurface)
        return;

    m_shellSurface->close();
}

SurfaceWrapper *SurfaceWrapper::stackFirstSurface() const
{
    return m_subSurfaces.isEmpty() ? const_cast<SurfaceWrapper *>(this)
                                   : m_subSurfaces.first()->stackFirstSurface();
}

SurfaceWrapper *SurfaceWrapper::stackLastSurface() const
{
    return m_subSurfaces.isEmpty() ? const_cast<SurfaceWrapper *>(this)
                                   : m_subSurfaces.last()->stackLastSurface();
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
        auto s = qobject_cast<SurfaceWrapper *>(item);
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
                                                        myIndex < siblingIndex ? siblingIndex - 1
                                                                               : siblingIndex);
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
        auto s = qobject_cast<SurfaceWrapper *>(item);
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
                                                        myIndex > siblingIndex ? siblingIndex + 1
                                                                               : siblingIndex);

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

void SurfaceWrapper::stackToFirst()
{
    if (!parentItem())
        return;

    if (m_parentSurface) {
        m_parentSurface->stackToFirst();
        stackBefore(m_parentSurface->stackFirstSurface());
    } else {
        auto first = parentItem()->childItems().first();
        stackBefore(first);
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
    Q_EMIT containerChanged();
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
    if (m_wrapperAboutToRemove)
        return;
    if (m_visibleDecoration == newVisibleDecoration)
        return;
    m_visibleDecoration = newVisibleDecoration;
    updateBoundingRect();
    Q_EMIT visibleDecorationChanged();
}

bool SurfaceWrapper::clipInOutput() const
{
    return m_clipInOutput;
}

void SurfaceWrapper::setClipInOutput(bool newClipInOutput)
{
    if (m_wrapperAboutToRemove)
        return;

    if (m_clipInOutput == newClipInOutput)
        return;
    m_clipInOutput = newClipInOutput;
    updateClipRect();
    Q_EMIT clipInOutputChanged();
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

    return m_noTitleBar || m_noDecoration;
}

void SurfaceWrapper::setNoTitleBar(bool newNoTitleBar)
{
    if (m_wrapperAboutToRemove)
        return;

    if (m_noTitleBar == newNoTitleBar)
        return;

    m_noTitleBar = newNoTitleBar;

    updateTitleBar();
    refreshConfiguredInitialXdgMaximize();
}

bool SurfaceWrapper::noCornerRadius() const
{
    return m_noCornerRadius;
}

void SurfaceWrapper::setNoCornerRadius(bool newNoCornerRadius)
{
    if (m_wrapperAboutToRemove)
        return;

    if (m_noCornerRadius == newNoCornerRadius)
        return;
    m_noCornerRadius = newNoCornerRadius;
    Q_EMIT noCornerRadiusChanged();
}

QRect SurfaceWrapper::iconGeometry() const
{
    return m_iconGeometry;
}

void SurfaceWrapper::setIconGeometry(QRect newIconGeometry)
{
    if (m_iconGeometry == newIconGeometry)
        return;
    m_iconGeometry = newIconGeometry;
    Q_EMIT iconGeometryChanged();
}

int SurfaceWrapper::workspaceId() const
{
    return m_workspaceId;
}

void SurfaceWrapper::setWorkspaceId(int newWorkspaceId)
{
    if (m_workspaceId == newWorkspaceId)
        return;

    bool onAllWorkspaceHasChanged = (m_workspaceId == Workspace::ShowOnAllWorkspaceId)
        != (newWorkspaceId == Workspace::ShowOnAllWorkspaceId);
    m_workspaceId = newWorkspaceId;

    if (onAllWorkspaceHasChanged)
        Q_EMIT showOnAllWorkspaceChanged();
    Q_EMIT workspaceIdChanged();
}

void SurfaceWrapper::setHideByWorkspace(bool hide)
{
    if (m_hideByWorkspace == hide)
        return;

    m_hideByWorkspace = hide;
    updateVisible();
}

bool SurfaceWrapper::alwaysOnTop() const
{
    return m_alwaysOnTop;
}

bool SurfaceWrapper::effectiveAlwaysOnTop() const
{
    return m_explicitAlwaysOnTop > 0;
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
    if (m_type == Type::Layer || m_type == Type::XdgPopup || isInputPopupLike()) [[unlikely]]
        return true;
    return m_workspaceId == Workspace::ShowOnAllWorkspaceId;
}

bool SurfaceWrapper::showOnWorkspace(int workspaceIndex) const
{
    if (m_workspaceId == workspaceIndex)
        return true;
    return showOnAllWorkspace();
}

bool SurfaceWrapper::isResizable() const
{
    return m_resizable;
}

bool SurfaceWrapper::isMaximizable() const
{
    return m_maximizable;
}

bool SurfaceWrapper::modal() const
{
    return m_modal;
}

void SurfaceWrapper::setModal(bool modal)
{
    if (m_modal == modal)
        return;
    m_modal = modal;
    Q_EMIT modalChanged();
}

SurfaceWrapper *SurfaceWrapper::findModal() const
{
    if (m_wrapperAboutToRemove)
        return nullptr;
    for (auto *child : std::as_const(m_subSurfaces)) {
        if (child->m_wrapperAboutToRemove)
            continue;
        if (child->modal()) {
            if (SurfaceWrapper *deepModal = child->findModal())
                return deepModal;
            return child;
        }
        if (SurfaceWrapper *deepModal = child->findModal())
            return deepModal;
    }
    return nullptr;
}

bool SurfaceWrapper::blur() const
{
    return m_blur;
}

void SurfaceWrapper::setBlur(bool blur)
{
    if (m_blur == blur) {
        return;
    }

    m_blur = blur;

    Q_EMIT blurChanged();
}

bool SurfaceWrapper::coverEnabled() const
{
    return m_coverContent;
}

void SurfaceWrapper::setCoverEnabled(bool coverEnabled)
{
    if (m_coverContent && !coverEnabled) {
        m_coverContent->setVisible(false);
        m_coverContent->deleteLater();
        m_coverContent = nullptr;
    } else if (coverEnabled && !m_coverContent) {
        m_coverContent = m_engine->createLaunchpadCover(this, m_ownsOutput, container());
        m_coverContent->setVisible(isVisible());
    }

    Q_EMIT coverEnabledChanged();
}

bool SurfaceWrapper::socketEnabled() const
{
    return m_socketEnabled;
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

bool SurfaceWrapper::hasInitializeContainer() const
{
    return m_hasActiveCapability.testFlag(ActiveControlState::HasInitializeContainer);
}

bool SurfaceWrapper::hasFocusCapability() const
{
    return m_focusControlStates == FocusControlState::Full;
}

void SurfaceWrapper::setHasInitializeContainer(bool value)
{
    Q_ASSERT(!value || m_container != nullptr);
    updateHasActiveCapability(ActiveControlState::HasInitializeContainer, value);
    Q_EMIT hasInitializeContainerChanged();

    if (m_prelaunchSplash && value) {
        // Start open animation when container initialized
        // m_prelaunchSplash can't get mapped signal
        createNewOrClose(OPEN_ANIMATION);
    }

    if (value) {
        tryApplyInitialMaximize();
        tryApplyDeferredSurfaceState();
    }
}

void SurfaceWrapper::disableWindowAnimation(bool disable)
{
    m_windowAnimationEnabled = !disable;
}

void SurfaceWrapper::updateExplicitAlwaysOnTop()
{
    int newExplicitAlwaysOnTop = m_alwaysOnTop;
    if (m_parentSurface)
        newExplicitAlwaysOnTop += m_parentSurface->m_explicitAlwaysOnTop;

    if (m_explicitAlwaysOnTop == newExplicitAlwaysOnTop)
        return;

    m_explicitAlwaysOnTop = newExplicitAlwaysOnTop;
    setZ(m_explicitAlwaysOnTop ? ALWAYSONTOPLAYER : 0);
    for (const auto &sub : std::as_const(m_subSurfaces))
        sub->updateExplicitAlwaysOnTop();
}

void SurfaceWrapper::updateSizeCapabilities()
{
    if (!m_shellSurface) {
        return;
    }

    const bool resizable = m_shellSurface->hasCapability(WToplevelSurface::Capability::Resize);
    const bool maximizable = m_shellSurface->hasCapability(WToplevelSurface::Capability::Maximized);

    if (m_resizable != resizable) {
        m_resizable = resizable;
        Q_EMIT resizableChanged();
    }

    if (m_maximizable != maximizable) {
        m_maximizable = maximizable;
        Q_EMIT maximizableChanged();
    }
}

void SurfaceWrapper::updateHasActiveCapability(ActiveControlState state, bool value)
{
    bool oldValue = hasActiveCapability();
    m_hasActiveCapability.setFlag(state, value);
    if (oldValue != hasActiveCapability()) {
        if (hasActiveCapability())
            Q_EMIT activationRequested();
        else
            Q_EMIT inactivationRequested();
    }
}

void SurfaceWrapper::updateActivateCapability()
{
    updateHasActiveCapability(ActiveControlState::HasActivateCapability,
                                m_shellSurface
                                    && m_shellSurface->hasCapability(WToplevelSurface::Capability::Activate));
}

void SurfaceWrapper::updateFocusCapability()
{
    updateFocusControlState(FocusControlState::HasFocusCapability,
                            m_shellSurface && m_shellSurface->hasCapability(WToplevelSurface::Capability::Focus));
}

void SurfaceWrapper::updateFocusControlState(FocusControlState state, bool value)
{
    auto old = hasFocusCapability();
    m_focusControlStates.setFlag(state, value);
    auto now = hasFocusCapability();
    if (old != now) {
        if (m_surfaceItem)
            m_surfaceItem->setFocusPolicy(now ? Qt::StrongFocus : Qt::NoFocus);
        Q_EMIT hasFocusCapabilityChanged();
    }
}

bool SurfaceWrapper::hasActiveCapability() const
{
    return m_hasActiveCapability == ActiveControlState::Full;
}

bool SurfaceWrapper::hasCapability(WToplevelSurface::Capability cap) const
{
    // SplashScreen doesn't have a real shellSurface
    if (m_type == Type::SplashScreen) {
        if (cap == WToplevelSurface::Capability::Activate)
            return true;
        // Focus, Maximized, FullScreen, Resize
        return false;
    }
    return m_shellSurface && m_shellSurface->hasCapability(cap);
}

bool SurfaceWrapper::skipSwitcher() const
{
    return m_skipSwitcher;
}

void SurfaceWrapper::setSkipSwitcher(bool skip)
{
    if (m_skipSwitcher == skip)
        return;

    m_skipSwitcher = skip;
    Q_EMIT skipSwitcherChanged();
}

bool SurfaceWrapper::skipDockPreView() const
{
    return m_skipDockPreView;
}

void SurfaceWrapper::setSkipDockPreView(bool skip)
{
    if (!skip && (m_type != Type::XdgToplevel && m_type != Type::XWayland)) {
        qCWarning(lcTlSurface) << "Only XdgToplevel or XWayland surfaces are allowed to set "
                                      "`skipDockPreView` to false.";
        return;
    }

    if (m_skipDockPreView == skip)
        return;

    m_skipDockPreView = skip;
    Q_EMIT skipDockPreViewChanged();
}

bool SurfaceWrapper::skipMutiTaskView() const
{
    return m_skipMutiTaskView;
}

void SurfaceWrapper::setSkipMutiTaskView(bool skip)
{
    if (m_skipMutiTaskView == skip)
        return;

    m_skipMutiTaskView = skip;
    Q_EMIT skipMutiTaskViewChanged();
}

bool SurfaceWrapper::isDDEShellSurface() const
{
    return m_isDdeShellSurface;
}

void SurfaceWrapper::setIsDDEShellSurface(bool value)
{
    if (m_isDdeShellSurface == value)
        return;

    m_isDdeShellSurface = value;
    Q_EMIT isDDEShellSurfaceChanged();
}

SurfaceWrapper::SurfaceRole SurfaceWrapper::surfaceRole() const
{
    return m_surfaceRole;
}

void SurfaceWrapper::setSurfaceRole(SurfaceRole role)
{
    if (m_surfaceRole == role)
        return;

    m_surfaceRole = role;
    if (role != SurfaceRole::Normal) {
        setZ(ALWAYSONTOPLAYER + static_cast<int>(role));
        for (const auto &sub : std::as_const(m_subSurfaces))
            sub->setZ(ALWAYSONTOPLAYER + static_cast<int>(role));
    } else {
        setZ(0);
        for (const auto &sub : std::as_const(m_subSurfaces))
            sub->setZ(0);
    }

    Q_EMIT surfaceRoleChanged();
}

quint32 SurfaceWrapper::autoPlaceYOffset() const
{
    return m_autoPlaceYOffset;
}

void SurfaceWrapper::setAutoPlaceYOffset(quint32 offset)
{
    if (m_autoPlaceYOffset == offset)
        return;

    m_autoPlaceYOffset = offset;
    setPositionAutomatic(offset == 0);
    Q_EMIT autoPlaceYOffsetChanged();
}

QPoint SurfaceWrapper::clientRequstPos() const
{
    return m_clientRequstPos;
}

void SurfaceWrapper::setClientRequstPos(QPoint pos)
{
    if (m_clientRequstPos == pos)
        return;

    m_clientRequstPos = pos;
    setPositionAutomatic(pos.isNull());
    Q_EMIT clientRequstPosChanged();
}

QPointF SurfaceWrapper::alignToPixelGrid(const QPointF &pos) const
{
    qreal devicePixelRatio = getOutputDevicePixelRatio(pos);
    qreal alignedX = std::round(pos.x() * devicePixelRatio) / devicePixelRatio;
    qreal alignedY = std::round(pos.y() * devicePixelRatio) / devicePixelRatio;
    return QPointF(alignedX, alignedY);
}

QRectF SurfaceWrapper::alignGeometryToPixelGrid(const QRectF &geometry) const
{
    qreal devicePixelRatio = getOutputDevicePixelRatio(geometry.center());
    qreal alignedX = std::round(geometry.x() * devicePixelRatio) / devicePixelRatio;
    qreal alignedY = std::round(geometry.y() * devicePixelRatio) / devicePixelRatio;
    qreal alignedWidth = std::round(geometry.width() * devicePixelRatio) / devicePixelRatio;
    qreal alignedHeight = std::round(geometry.height() * devicePixelRatio) / devicePixelRatio;

    QRectF result(alignedX, alignedY, alignedWidth, alignedHeight);
    return result;
}

qreal SurfaceWrapper::getOutputDevicePixelRatio(const QPointF &pos) const
{
    if (surface() && !surface()->outputs().isEmpty()) {
        const auto &outputs = surface()->outputs();

        if (outputs.size() == 1) {
            return outputs.first()->scale();
        }

        for (auto woutput : outputs) {
            if (woutput && woutput->isEnabled()) {
                QRectF outputGeometry(woutput->position(), woutput->size());
                if (outputGeometry.contains(pos)) {
                    return woutput->scale();
                }
            }
        }
    }

    return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}
