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

#include <QVariant>
#include <QColor>

#include <winputpopupsurfaceitem.h>
#include <wlayersurface.h>
#include <wlayersurfaceitem.h>
#include <woutput.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wsocket.h>
#include <wxdgpopupsurfaceitem.h>
#include <wxdgtoplevelsurfaceitem.h>
#include <wxwaylandsurface.h>
#include <wxwaylandsurfaceitem.h>

#include <qwlayershellv1.h>
#include <qwbuffer.h>

#define OPEN_ANIMATION 1
#define CLOSE_ANIMATION 2
#define ALWAYSONTOPLAYER 1

SurfaceWrapper::SurfaceWrapper(QmlEngine *qmlEngine,
                               WToplevelSurface *shellSurface,
                               Type type,
                               const QString &appId,
                               QQuickItem *parent)
    : QQuickItem(parent)
    , m_engine(qmlEngine)
    , m_shellSurface(shellSurface)
    , m_type(type)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_titleBarState(TitleBarState::Default)
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
    , m_appId(appId)
{
    QQmlEngine::setContextForObject(this, qmlEngine->rootContext());

    setup();
}

SurfaceWrapper::SurfaceWrapper(SurfaceWrapper *original,
                               QQuickItem *parent)
    : QQuickItem(parent)
    , m_engine(original->m_engine)
    , m_shellSurface(original->m_shellSurface)
    , m_type(original->m_type)
    , m_positionAutomatic(true)
    , m_visibleDecoration(true)
    , m_clipInOutput(false)
    , m_noDecoration(true)
    , m_titleBarState(TitleBarState::Default)
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

        m_prelaunchSplash = m_engine->createPrelaunchSplash(this,
             original->radius(),
             iconVar.value<QW_NAMESPACE::qw_buffer *>(),
             bgColor);
        setNoDecoration(false);

        connect(original,
            &SurfaceWrapper::surfaceItemCreated,
                this,
                [this, original]() {
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
    , m_titleBarState(TitleBarState::Default)
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
    , m_appId(appId)
{
    QQmlEngine::setContextForObject(this, qmlEngine->rootContext());
    if (initialSize.isValid() && initialSize.width() > 0 && initialSize.height() > 0) {
        // Also set implicit size to keep QML layout consistent
        setImplicitSize(initialSize.width(), initialSize.height());
        qCDebug(treelandSurface) << "Prelaunch Splash: set initial size to" << initialSize;
    } else {
        setImplicitSize(800, 600);
    }
    m_prelaunchSplash = m_engine->createPrelaunchSplash(this, radius(), iconBuffer, backgroundColor);
    // Connect to QML signal so C++ can destroy the QML item when requested
    connect(m_prelaunchSplash,
            SIGNAL(destroyRequested()),
            this,
            SLOT(onPrelaunchSplashDestroyRequested()));

    setNoDecoration(false);
    updateHasActiveCapability(ActiveControlState::MappedOrSplash, true);// Splash is true
}

SurfaceWrapper::~SurfaceWrapper()
{
    Q_ASSERT(!m_ownsOutput);
    Q_ASSERT(!m_container);
    Q_ASSERT(!m_parentSurface);
    Q_ASSERT(m_subSurfaces.isEmpty());

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

    QQmlEngine::setContextForObject(m_surfaceItem, m_engine->rootContext());
    m_surfaceItem->setDelegate(m_engine->surfaceContentComponent());
    m_surfaceItem->setResizeMode(WSurfaceItem::ManualResize);
    m_surfaceItem->setShellSurface(m_shellSurface);

    if (!m_isProxy) {
        m_shellSurface->safeConnect(&WToplevelSurface::requestMinimize, this, [this]() {
            requestMinimize();
        });
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelMinimize, this, [this]() {
            requestCancelMinimize();
        });
        m_shellSurface->safeConnect(&WToplevelSurface::requestMaximize,
                                    this,
                                    &SurfaceWrapper::requestMaximize);
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelMaximize,
                                    this,
                                    &SurfaceWrapper::requestCancelMaximize);
        m_shellSurface->safeConnect(&WToplevelSurface::requestMove,
                                    this,
                                    &SurfaceWrapper::requestMove);
        m_shellSurface->safeConnect(&WToplevelSurface::requestResize,
                                    this,
                                    [this](WSeat *, Qt::Edges edge, quint32) {
                                        Q_EMIT requestResize(edge);
                                    });
        m_shellSurface->safeConnect(&WToplevelSurface::requestFullscreen,
                                    this,
                                    &SurfaceWrapper::requestFullscreen);
        m_shellSurface->safeConnect(&WToplevelSurface::requestCancelFullscreen,
                                    this,
                                    &SurfaceWrapper::requestCancelFullscreen);

        if (m_type == Type::XdgToplevel) {
            m_shellSurface->safeConnect(&WToplevelSurface::requestShowWindowMenu,
                                        this,
                                        [this](WSeat *, QPoint pos, quint32) {
                                            Q_EMIT requestShowWindowMenu(
                                                { pos.x() + m_surfaceItem->leftPadding(),
                                                  pos.y() + m_surfaceItem->topPadding() });
                                        });
        }
    }
    m_shellSurface->surface()->safeConnect(&WSurface::mappedChanged,
                                           this,
                                           &SurfaceWrapper::onMappedChanged);

    Q_EMIT surfaceItemCreated();

    // Forward WToplevelSurface appId changes when using fallback mode
    if (m_appId.isEmpty()) {
        m_shellSurface->safeConnect(&WToplevelSurface::appIdChanged, this, [this]() {
            Q_EMIT appIdChanged();
        });
    }

    if (!m_prelaunchSplash || !m_prelaunchSplash->isVisible()) {
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

    if (!m_shellSurface->hasCapability(WToplevelSurface::Capability::Focus)) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        m_surfaceItem->setFocusPolicy(Qt::NoFocus);
#endif
    }

    if (m_type == Type::XdgToplevel && !m_isProxy) // x11 will set later
        setSkipDockPreView(false);

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

        auto updateX11shouldSkipDock = [this]() {
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
        };

        connect(xwaylandSurface,
                &WXWaylandSurface::bypassManagerChanged,
                this,
                updateX11shouldSkipDock);
        connect(xwaylandSurface,
                &WXWaylandSurface::windowTypesChanged,
                this,
                updateX11shouldSkipDock);
        updateX11shouldSkipDock();
    }
}

void SurfaceWrapper::convertToNormalSurface(WToplevelSurface *shellSurface, Type type)
{
    // Conversion only allowed from prelaunch (SplashScreen) state
    if (m_type != Type::SplashScreen || m_shellSurface != nullptr) {
        qCCritical(treelandSurface) << "convertToNormalSurface can only be called on prelaunch surfaces";
        return;
    }

    // Assign new shell surface (QPointer auto-detects destruction)
    m_shellSurface = shellSurface;
    m_type = type;
    Q_EMIT typeChanged();

    // Call setup() to initialize surfaceItem related features
    setup();

    // If outputs were set during prelaunch, apply them now
    if (m_prelaunchOutputs.size() > 0) {
        setOutputs(m_prelaunchOutputs);
        m_prelaunchOutputs.clear();
    }

    // setNoDecoration not called updateTitleBar when type is SplashScreen
    updateTitleBar();
    updateDecoration();
    updateActiveState();
    if (surface()->mapped()) {
        Q_ASSERT(m_prelaunchSplash);
        QMetaObject::invokeMethod(m_prelaunchSplash, "hideAndDestroy", Qt::QueuedConnection);
    } // else hideAndDestroy will be called in onMappedChanged
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
    // No shellSurface in prelaunch mode -> early return
    if (m_shellSurface)
        updateActiveState();

    Q_EMIT isActivatedChanged();
}

void SurfaceWrapper::updateActiveState()
{
    if (!m_shellSurface) {
        qCCritical(treelandSurface) << "updateActiveState called without a valid shellSurface";
        return;
    }
    m_shellSurface->setActivate(m_isActivated);
    auto parent = parentSurface();
    while (parent) {
        if (!parent->hasActiveCapability()) {
            // Maybe it's parent is Minimized or Unmapped
            break;
        }
        parent->setActivate(m_isActivated);
        parent = parent->parentSurface();
    }
}

void SurfaceWrapper::setFocus(bool focus, Qt::FocusReason reason)
{
    // No surfaceItem in prelaunch mode -> early return
    if (!m_surfaceItem)
        return;

    if (focus)
        m_surfaceItem->forceActiveFocus(reason);
    else
        m_surfaceItem->setFocus(false, reason);
}

void SurfaceWrapper::onPrelaunchSplashDestroyRequested()
{
    if (m_surfaceItem) {
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
        if (m_decoration)
            m_decoration->stackBefore(m_surfaceItem);
    }

    updateVisible();

    Q_ASSERT(m_prelaunchSplash);
    m_prelaunchSplash->deleteLater();
    m_prelaunchSplash = nullptr;
    updateHasActiveCapability(ActiveControlState::MappedOrSplash,
                                surface() && surface()->mapped());
    if (m_shellSurface) 
        updateActiveState();
    Q_EMIT prelaunchSplashChanged();
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

bool SurfaceWrapper::resize(const QSizeF &size)
{
    // No surfaceItem in prelaunch mode -> return false
    if (!m_surfaceItem)
        return false;

    return m_surfaceItem->resizeSurface(size);
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

void SurfaceWrapper::setNormalGeometry(const QRectF &newNormalGeometry)
{
    if (m_normalGeometry == newNormalGeometry)
        return;
    m_normalGeometry = newNormalGeometry;
    Q_EMIT normalGeometryChanged();
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

    if (m_surfaceState == State::Maximized) {
        setPosition(newMaximizedGeometry.topLeft());
        resize(newMaximizedGeometry.size());
    } else if (m_pendingState == State::Maximized && m_geometryAnimation) {
        m_geometryAnimation->setProperty("targetGeometry", newMaximizedGeometry);
    }

    Q_EMIT maximizedGeometryChanged();
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

    if (m_surfaceState == State::Fullscreen) {
        setPosition(newFullscreenGeometry.topLeft());
        resize(newFullscreenGeometry.size());
    } else if (m_pendingState == State::Fullscreen && m_geometryAnimation) {
        m_geometryAnimation->setProperty("targetGeometry", newFullscreenGeometry);
    }

    Q_EMIT fullscreenGeometryChanged();

    updateClipRect();
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
        qCDebug(treelandSurface) << "SurfaceWrapper::setOutputs called but surface() is null!";
        return;
    }
    auto oldOutputs = surface()->outputs();
    for (auto output : oldOutputs) {
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

    if (m_geometryAnimation)
        return;

    if (m_surfaceState == newSurfaceState)
        return;

    if (container()->filterSurfaceStateChange(this, newSurfaceState, m_surfaceState))
        return;

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

    if (targetGeometry.isValid()) {
        startStateChangeAnimation(newSurfaceState, targetGeometry);
    } else {
        if (m_geometryAnimation) {
            m_geometryAnimation->disconnect(this);
            m_geometryAnimation->deleteLater();
        }

        doSetSurfaceState(newSurfaceState);
    }
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

void SurfaceWrapper::markWrapperToRemoved()
{
    Q_ASSERT_X(!m_wrapperAboutToRemove, Q_FUNC_INFO, "Can't call `markWrapperToRemoved` twice!");
    m_wrapperAboutToRemove = true;
    Q_EMIT aboutToBeInvalidated();

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
    m_subSurfaces.clear();
    m_shellSurface = nullptr;
    if (m_surfaceItem)
        m_surfaceItem->disconnect(this);

    if (!isWindowAnimationRunning()) {
        deleteLater();
    } // else delete this in Animation(for window close animation) finish
}

bool SurfaceWrapper::acceptKeyboardFocus() const
{
    return m_acceptKeyboardFocus;
}

void SurfaceWrapper::setAcceptKeyboardFocus(bool accept)
{
    if (m_acceptKeyboardFocus == accept)
        return;

    m_acceptKeyboardFocus = accept;
    Q_EMIT acceptKeyboardFocusChanged();
}

bool SurfaceWrapper::isActivated() const
{
    return m_isActivated;
}

void SurfaceWrapper::setNoDecoration(bool newNoDecoration)
{
    if (m_wrapperAboutToRemove)
        return;

    if (m_noDecoration == newNoDecoration)
        return;

    m_noDecoration = newNoDecoration;
    setNoCornerRadius(newNoDecoration);

    if (m_type == Type::SplashScreen && !m_isProxy) {
        Q_EMIT noDecorationChanged();
        return;
    }

    updateDecoration();
}

void SurfaceWrapper::updateDecoration()
{
    if (m_titleBarState == TitleBarState::Default && m_type != Type::SplashScreen)
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
    if (m_prelaunchSplash && m_prelaunchSplash->isVisible())
        return;

    setVisible(!m_hideByWorkspace && !isMinimized() && (surface() && surface()->mapped())
               && m_socketEnabled && m_hideByshowDesk && !m_confirmHideByLockScreen
               && Helper::instance()->surfaceBelongsToCurrentSession(this));
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

    if (isNormal() && !m_geometryAnimation) {
        setNormalGeometry(newGeometry);
    }

    if (widthValid() && heightValid()) {
        resize(newGeometry.size());
    }

    Q_EMIT geometryChanged();
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        updateBoundingRect();
    updateClipRect();
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

void SurfaceWrapper::doSetSurfaceState(State newSurfaceState)
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

    setVisibleDecoration(newSurfaceState == State::Minimized || newSurfaceState == State::Normal);
    setNoCornerRadius(newSurfaceState == State::Maximized || newSurfaceState == State::Fullscreen
                      || newSurfaceState == State::Tiling);

    m_previousSurfaceState.setValueBypassingBindings(m_surfaceState);
    m_surfaceState.setValueBypassingBindings(newSurfaceState);

    switch (m_previousSurfaceState.value()) {
    case State::Maximized:
        m_shellSurface->setMaximize(false);
        break;
    case State::Minimized:
        m_shellSurface->setMinimize(false);
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
    m_previousSurfaceState.notify();

    switch (m_surfaceState.value()) {
    case State::Maximized:
        m_shellSurface->setMaximize(true);
        break;
    case State::Minimized:
        m_shellSurface->setMinimize(true);
        updateHasActiveCapability(ActiveControlState::UnMinimized, false);
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
    m_surfaceState.notify();
    updateTitleBar();
    updateVisible();
}

void SurfaceWrapper::onAnimationReady()
{
    Q_ASSERT(m_pendingState != m_surfaceState);
    Q_ASSERT(m_pendingGeometry.isValid());

    if (!resize(m_pendingGeometry.size())) {
        // abort change state if resize failed
        m_geometryAnimation->disconnect(this);
        m_geometryAnimation->deleteLater();
        return;
    }

    QPointF alignedPos = alignToPixelGrid(m_pendingGeometry.topLeft());
    setPosition(alignedPos);
    doSetSurfaceState(m_pendingState);
}

void SurfaceWrapper::onAnimationFinished()
{
    setXwaylandPositionFromSurface(true);
    Q_ASSERT(m_geometryAnimation);
    m_geometryAnimation->disconnect(this);
    m_geometryAnimation->deleteLater();
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
    onWindowAnimationFinished();
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

    if (m_prelaunchSplash) {
        // The QML part will check for duplicate calls.
        QMetaObject::invokeMethod(m_prelaunchSplash, "hideAndDestroy", Qt::QueuedConnection);
    }

    if (!m_isProxy) {
        if (mapped) {
            if (!m_prelaunchSplash)
                createNewOrClose(OPEN_ANIMATION);
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

    updateVisible();
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
    updateVisible();
}

void SurfaceWrapper::startShowDesktopAnimation(bool show)
{

    if (m_showDesktopAnimation) {
        m_showDesktopAnimation->disconnect(this);
        m_showDesktopAnimation->deleteLater();
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
    // TODO: move to dconfig
    if (m_type == Type::InputPopup)
        return 0;
    if (m_type == Type::XdgPopup)
        return 8;

    qreal radius = m_radius;

    // TODO: Handle: XdgToplevel, popup, InputPopup, XWayland (bypass, window type:
    // menu/normal/popup)
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

void SurfaceWrapper::requestMinimize(bool onAnimation)
{
    setSurfaceState(State::Minimized);
    if (onAnimation)
        startMinimizeAnimation(iconGeometry(), CLOSE_ANIMATION);
}

void SurfaceWrapper::requestCancelMinimize(bool onAnimation)
{
    if (m_surfaceState != State::Minimized && m_hideByshowDesk)
        return;
    if (!m_hideByshowDesk)
        setHideByShowDesk(true);

    doSetSurfaceState(m_previousSurfaceState);
    if (onAnimation)
        startMinimizeAnimation(iconGeometry(), OPEN_ANIMATION);
}

void SurfaceWrapper::requestMaximize()
{
    if (m_surfaceState == State::Minimized || m_surfaceState == State::Fullscreen)
        return;

    setSurfaceState(State::Maximized);
}

void SurfaceWrapper::requestCancelMaximize()
{
    if (m_surfaceState != State::Maximized)
        return;

    setSurfaceState(State::Normal);
}

void SurfaceWrapper::requestToggleMaximize()
{
    if (m_surfaceState == State::Maximized)
        requestCancelMaximize();
    else
        requestMaximize();
}

void SurfaceWrapper::requestFullscreen()
{
    if (m_surfaceState == State::Minimized)
        return;

    setSurfaceState(State::Fullscreen);
}

void SurfaceWrapper::requestCancelFullscreen()
{
    if (m_surfaceState != State::Fullscreen)
        return;

    setSurfaceState(m_previousSurfaceState);
}

void SurfaceWrapper::requestClose()
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
    if (m_titleBarState == TitleBarState::Visible)
        return false;

    return m_titleBarState == TitleBarState::Hidden || m_noDecoration;
}

void SurfaceWrapper::setNoTitleBar(bool newNoTitleBar)
{
    if (m_wrapperAboutToRemove)
        return;

    if (newNoTitleBar) {
        m_titleBarState = TitleBarState::Hidden;
    } else {
        m_titleBarState = TitleBarState::Visible;
    }

    if (!m_shellSurface) // has prelaunchSplash
        return;

    updateTitleBar();
}

void SurfaceWrapper::resetNoTitleBar()
{
    m_titleBarState = TitleBarState::Default;
    updateTitleBar();
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
    if (m_type == Type::Layer || m_type == Type::XdgPopup || m_type == Type::InputPopup)
        [[unlikely]]
        return true;
    return m_workspaceId == Workspace::ShowOnAllWorkspaceId;
}

bool SurfaceWrapper::showOnWorkspace(int workspaceIndex) const
{
    if (m_workspaceId == workspaceIndex)
        return true;
    return showOnAllWorkspace();
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

void SurfaceWrapper::updateHasActiveCapability(ActiveControlState state, bool value)
{
    bool oldValue = hasActiveCapability();
    m_hasActiveCapability.setFlag(state, value);
    if (oldValue != hasActiveCapability()) {
        if (hasActiveCapability())
            Q_EMIT requestActive();
        else
            Q_EMIT requestInactive();
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
    if (m_type != Type::XdgToplevel && m_type != Type::XWayland) {
        qCWarning(treelandSurface) << "Only xdgtoplevel and x11 surface can `setSkipDockPreView`";
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
