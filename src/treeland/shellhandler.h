#include <wglobal.h>

#include <qwglobal.h>

#include <QObject>

Q_MOC_INCLUDE("workspace.h")

class Helper;
class SurfaceWrapper;
class RootSurfaceContainer;
class LayerSurfaceContainer;
class Workspace;
class SurfaceContainer;
class QmlEngine;
class DDEShellManagerV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WXdgSurface;
class WXdgShell;
class WLayerShell;
class WLayerSurface;
class WXWayland;
class WInputMethodHelper;
class WInputPopupSurface;
class WSeat;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_compositor;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QQuickWindow;
QT_END_NAMESPACE

class ShellHandler : public QObject
{
    friend class Helper;
    Q_OBJECT

public:
    ShellHandler(RootSurfaceContainer *rootContainer);
    [[nodiscard]] Workspace *workspace() const;

    void createComponent(QmlEngine *engine);
    void initXdgShell(WAYLIB_SERVER_NAMESPACE::WServer *server, DDEShellManagerV1 *ddeShellV1);
    void initLayerShell(WAYLIB_SERVER_NAMESPACE::WServer *server);
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXWayland *createXWayland(
        WAYLIB_SERVER_NAMESPACE::WServer *server,
        WAYLIB_SERVER_NAMESPACE::WSeat *seat,
        QW_NAMESPACE::qw_compositor *compositor,
        bool lazy);
    // FIXME: never call removeXWayland in treeland.cpp
    void removeXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland);
    void initInputMethodHelper(WAYLIB_SERVER_NAMESPACE::WServer *server,
                               WAYLIB_SERVER_NAMESPACE::WSeat *seat);

    WAYLIB_SERVER_NAMESPACE::WXWayland *defaultXWaylandSocket() const;
Q_SIGNALS:
    void surfaceWrapperAdded(SurfaceWrapper *wrapper);
    void surfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);

private Q_SLOTS:
    void onXdgSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgSurface *surface);
    void onXdgSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgSurface *surface);

    void onLayerSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);
    void onLayerSurfaceRemoved(WAYLIB_SERVER_NAMESPACE::WLayerSurface *surface);

    void onInputPopupSurfaceV2Added(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);
    void onInputPopupSurfaceV2Removed(WAYLIB_SERVER_NAMESPACE::WInputPopupSurface *surface);

private:
    void setupSurfaceActiveWatcher(SurfaceWrapper *wrapper);
    void setupSurfaceWindowMenu(SurfaceWrapper *wrapper);
    void updateLayerSurfaceContainer(SurfaceWrapper *surface);
    void handleDdeShellSurfaceAdded(WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                                    SurfaceWrapper *wrapper);

    WAYLIB_SERVER_NAMESPACE::WXdgShell *m_xdgShell = nullptr;
    WAYLIB_SERVER_NAMESPACE::WLayerShell *m_layerShell = nullptr;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *m_inputMethodHelper = nullptr;
    QList<WAYLIB_SERVER_NAMESPACE::WXWayland *> m_xwaylands;

    RootSurfaceContainer *m_rootSurfaceContainer = nullptr;
    LayerSurfaceContainer *m_backgroundContainer = nullptr;
    LayerSurfaceContainer *m_bottomContainer = nullptr;
    Workspace *m_workspace = nullptr;
    LayerSurfaceContainer *m_topContainer = nullptr;
    LayerSurfaceContainer *m_overlayContainer = nullptr;
    SurfaceContainer *m_popupContainer = nullptr;
    QObject *m_windowMenu = nullptr;

    DDEShellManagerV1 *m_refDDEShellV1 = nullptr;
};
