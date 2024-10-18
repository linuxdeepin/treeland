#include "shellhandler.h"

#include "ddeshellmanagerv1.h"
#include "helper.h"
#include "layersurfacecontainer.h"
#include "qmlengine.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"
#include "workspace.h"

#include <winputmethodhelper.h>
#include <winputpopupsurface.h>
#include <wlayershell.h>
#include <wlayersurface.h>
#include <wserver.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <qwcompositor.h>
#include <qwxwaylandsurface.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

ShellHandler::ShellHandler(RootSurfaceContainer *rootContainer)
    : m_rootSurfaceContainer(rootContainer)
    , m_backgroundContainer(new LayerSurfaceContainer(rootContainer))
    , m_bottomContainer(new LayerSurfaceContainer(rootContainer))
    , m_workspace(new Workspace(rootContainer))
    , m_topContainer(new LayerSurfaceContainer(rootContainer))
    , m_overlayContainer(new LayerSurfaceContainer(rootContainer))
    , m_popupContainer(new SurfaceContainer(rootContainer))
{
    m_backgroundContainer->setZ(RootSurfaceContainer::BackgroundZOrder);
    m_bottomContainer->setZ(RootSurfaceContainer::BottomZOrder);
    m_workspace->setZ(RootSurfaceContainer::NormalZOrder);
    m_topContainer->setZ(RootSurfaceContainer::TopZOrder);
    m_overlayContainer->setZ(RootSurfaceContainer::OverlayZOrder);
    m_popupContainer->setZ(RootSurfaceContainer::PopupZOrder);
}

Workspace *ShellHandler::workspace() const
{
    return m_workspace;
}

void ShellHandler::createComponent(QmlEngine *engine)
{
    m_windowMenu = engine->createWindowMenu(Helper::instance());
}

void ShellHandler::initXdgShell(WServer *server, DDEShellManagerV1 *ddeShellV1)
{
    Q_ASSERT_X(!m_xdgShell, Q_FUNC_INFO, "Only init once!");
    m_refDDEShellV1 = ddeShellV1;
    m_xdgShell = server->attach<WXdgShell>();
    connect(m_xdgShell, &WXdgShell::surfaceAdded, this, &ShellHandler::onXdgSurfaceAdded);
    connect(m_xdgShell, &WXdgShell::surfaceRemoved, this, &ShellHandler::onXdgSurfaceRemoved);
}

void ShellHandler::initLayerShell(WServer *server)
{
    Q_ASSERT_X(!m_layerShell, Q_FUNC_INFO, "Only init once!");
    m_layerShell = server->attach<WLayerShell>();
    connect(m_layerShell, &WLayerShell::surfaceAdded, this, &ShellHandler::onLayerSurfaceAdded);
    connect(m_layerShell, &WLayerShell::surfaceRemoved, this, &ShellHandler::onLayerSurfaceRemoved);
}

WXWayland *ShellHandler::createXWayland(WServer *server,
                                        WSeat *seat,
                                        qw_compositor *compositor,
                                        bool lazy)
{
    auto *xwayland = server->attach<WXWayland>(compositor, false);
    m_xwaylands.append(xwayland);
    xwayland->setSeat(seat);
    connect(xwayland, &WXWayland::surfaceAdded, this, [this, xwayland](WXWaylandSurface *surface) {
        surface->safeConnect(&qw_xwayland_surface::notify_associate, this, [this, surface] {
            auto wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                              surface,
                                              SurfaceWrapper::Type::XWayland);
            m_workspace->addSurface(wrapper);
            Q_ASSERT(wrapper->parentItem());
            setupSurfaceWindowMenu(wrapper);
            setupSurfaceActiveWatcher(wrapper);
            Q_EMIT surfaceWrapperAdded(wrapper);
        });
        surface->safeConnect(&qw_xwayland_surface::notify_dissociate, this, [this, surface] {
            auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
            Q_EMIT surfaceWrapperAboutToRemove(wrapper);
            m_rootSurfaceContainer->destroyForSurface(wrapper);
        });
    });

    return xwayland;
}

void ShellHandler::removeXWayland(WXWayland *xwayland)
{
    m_xwaylands.removeOne(xwayland);
    xwayland->safeDeleteLater();
}

void ShellHandler::initInputMethodHelper(WServer *server, WSeat *seat)
{
    Q_ASSERT_X(!m_inputMethodHelper, Q_FUNC_INFO, "Only init once!");
    m_inputMethodHelper = new WInputMethodHelper(server, seat);

    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Added,
            this,
            &ShellHandler::onInputPopupSurfaceV2Added);
    connect(m_inputMethodHelper,
            &WInputMethodHelper::inputPopupSurfaceV2Removed,
            this,
            &ShellHandler::onInputPopupSurfaceV2Removed);
}

void ShellHandler::onXdgSurfaceAdded(WXdgSurface *surface)
{
    SurfaceWrapper *wrapper = nullptr;

    if (surface->isToplevel()) {
        wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XdgToplevel);
        if (m_refDDEShellV1->isDdeShellSurface(surface->surface())) {
            handleDdeShellSurfaceAdded(surface->surface(), wrapper);
        }
    } else {
        wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                     surface,
                                     SurfaceWrapper::Type::XdgPopup);
    }

    if (surface->isPopup()) {
        auto parent = surface->parentSurface();
        auto parentWrapper = m_rootSurfaceContainer->getSurface(parent);
        parentWrapper->addSubSurface(wrapper);
        m_popupContainer->addSurface(wrapper);
        wrapper->setOwnsOutput(parentWrapper->ownsOutput());
    } else {
        auto updateSurfaceWithParentContainer = [this, wrapper, surface] {
            if (wrapper->parentSurface())
                wrapper->parentSurface()->removeSubSurface(wrapper);
            if (wrapper->container())
                wrapper->container()->removeSurface(wrapper);

            if (auto parent = surface->parentSurface()) {
                auto parentWrapper = m_rootSurfaceContainer->getSurface(parent);
                auto container = qobject_cast<Workspace *>(parentWrapper->container());
                Q_ASSERT(container);
                parentWrapper->addSubSurface(wrapper);
                container->addSurface(wrapper, parentWrapper->workspaceId());
            } else {
                m_workspace->addSurface(wrapper);
            }
        };

        surface->safeConnect(&WXdgSurface::parentXdgSurfaceChanged,
                             this,
                             updateSurfaceWithParentContainer);
        updateSurfaceWithParentContainer();
        setupSurfaceWindowMenu(wrapper);
        setupSurfaceActiveWatcher(wrapper);
    }

    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onXdgSurfaceRemoved(WXdgSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface);
    if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel) {
        if (m_refDDEShellV1->isDdeShellSurface(surface->surface())) {
            m_refDDEShellV1->ddeShellSurfaceFromWSurface(surface->surface())->destroy();
        }
    }
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::setupSurfaceActiveWatcher(SurfaceWrapper *wrapper)
{
    Q_ASSERT_X(wrapper->container(), Q_FUNC_INFO, "Must setContainer at first!");

    connect(wrapper, &SurfaceWrapper::requestActive, this, [this, wrapper]() {
        if (wrapper->showOnWorkspace(m_workspace->currentIndex()))
            Helper::instance()->activateSurface(wrapper);
        else
            m_workspace->current()->pushActivedSurface(wrapper);
    });

    connect(wrapper, &SurfaceWrapper::requestDeactive, this, [this, wrapper]() {
        m_workspace->removeActivedSurface(wrapper);
        Helper::instance()->activateSurface(m_workspace->current()->latestActiveSurface());
    });
}

void ShellHandler::onLayerSurfaceAdded(WLayerSurface *surface)
{
    auto wrapper =
        new SurfaceWrapper(Helper::instance()->qmlEngine(), surface, SurfaceWrapper::Type::Layer);
    wrapper->setSkipSwitcher(true);
    wrapper->setSkipDockPreView(true);
    wrapper->setSkipMutiTaskView(true);
    updateLayerSurfaceContainer(wrapper);

    connect(surface, &WLayerSurface::layerChanged, this, [this, wrapper] {
        updateLayerSurfaceContainer(wrapper);
    });

    setupSurfaceActiveWatcher(wrapper);
    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onLayerSurfaceRemoved(WLayerSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::updateLayerSurfaceContainer(SurfaceWrapper *surface)
{
    auto layer = qobject_cast<WLayerSurface *>(surface->shellSurface());
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

void ShellHandler::onInputPopupSurfaceV2Added(WInputPopupSurface *surface)
{
    auto wrapper = new SurfaceWrapper(Helper::instance()->qmlEngine(),
                                      surface,
                                      SurfaceWrapper::Type::InputPopup);
    auto parent = surface->parentSurface();
    auto parentWrapper = m_rootSurfaceContainer->getSurface(parent);
    parentWrapper->addSubSurface(wrapper);
    m_popupContainer->addSurface(wrapper);
    wrapper->setOwnsOutput(parentWrapper->ownsOutput());
    Q_ASSERT(wrapper->parentItem());
    Q_EMIT surfaceWrapperAdded(wrapper);
}

void ShellHandler::onInputPopupSurfaceV2Removed(WInputPopupSurface *surface)
{
    auto wrapper = m_rootSurfaceContainer->getSurface(surface->surface());
    Q_EMIT surfaceWrapperAboutToRemove(wrapper);
    m_rootSurfaceContainer->destroyForSurface(wrapper);
}

void ShellHandler::setupSurfaceWindowMenu(SurfaceWrapper *wrapper)
{
    Q_ASSERT(m_windowMenu);
    connect(wrapper,
            &SurfaceWrapper::requestShowWindowMenu,
            m_windowMenu,
            [this, wrapper](QPoint pos) {
                QMetaObject::invokeMethod(m_windowMenu,
                                          "showWindowMenu",
                                          QVariant::fromValue(wrapper),
                                          QVariant::fromValue(pos));
            });
}

void ShellHandler::handleDdeShellSurfaceAdded(WSurface *surface, SurfaceWrapper *wrapper)
{
    wrapper->setIsDdeShellSurface(true);
    auto ddeShellSurface = m_refDDEShellV1->ddeShellSurfaceFromWSurface(surface);
    Q_ASSERT(ddeShellSurface);
    auto updateLayer = [ddeShellSurface, wrapper] {
        if (ddeShellSurface->m_role.value() == treeland_dde_shell_surface::Role::OVERLAY)
            wrapper->setSurfaceRole(SurfaceWrapper::SurfaceRole::Overlay);
    };

    if (ddeShellSurface->m_role.has_value())
        updateLayer();

    connect(ddeShellSurface, &treeland_dde_shell_surface::roleChanged, this, [updateLayer] {
        updateLayer();
    });

    if (ddeShellSurface->m_yOffset.has_value())
        wrapper->setAutoPlaceYOffset(ddeShellSurface->m_yOffset.value());

    connect(ddeShellSurface,
            &treeland_dde_shell_surface::yOffsetChanged,
            this,
            [wrapper](uint32_t offset) {
                wrapper->setAutoPlaceYOffset(offset);
            });

    if (ddeShellSurface->m_surfacePos.has_value())
        wrapper->setClientRequstPos(ddeShellSurface->m_surfacePos.value());

    connect(ddeShellSurface,
            &treeland_dde_shell_surface::positionChanged,
            this,
            [wrapper](QPoint pos) {
                wrapper->setClientRequstPos(pos);
            });

    if (ddeShellSurface->m_skipSwitcher.has_value())
        wrapper->setSkipSwitcher(ddeShellSurface->m_skipSwitcher.value());

    if (ddeShellSurface->m_skipDockPreView.has_value())
        wrapper->setSkipDockPreView(ddeShellSurface->m_skipDockPreView.value());

    if (ddeShellSurface->m_skipMutiTaskView.has_value())
        wrapper->setSkipMutiTaskView(ddeShellSurface->m_skipMutiTaskView.value());

    connect(ddeShellSurface,
            &treeland_dde_shell_surface::skipSwitcherChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipSwitcher(skip);
            });
    connect(ddeShellSurface,
            &treeland_dde_shell_surface::skipDockPreViewChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipDockPreView(skip);
            });
    connect(ddeShellSurface,
            &treeland_dde_shell_surface::skipMutiTaskViewChanged,
            this,
            [wrapper](bool skip) {
                wrapper->setSkipMutiTaskView(skip);
            });
}
