// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capturemanager.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "modules/capture/capture.h"
#include "surface/surfacewrapper.h"
#include "output/output.h"
#include "common/treelandlogging.h"

#include <wserver.h>
#include <wsocket.h>
#include <wsurfaceitem.h>
#include <woutputrenderwindow.h>
#include <wextforeigntoplevellistv1.h>
#include <wextimagecapturesourcev1impl.h>

#include <wlr_all.h>

#include <type_traits>
#include <utility>

#define EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION 1
#define EXT_OUTPUT_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1
#define EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1

// wlr_ext_image_copy_capture_session_v1_set_paused() is a treeland wlroots
// extension. Builds against an external Waylib may use a wlroots without it,
// in which case capture sessions simply keep running unpaused.
namespace {

template<typename T, typename = void>
struct CaptureSessionPauseSupported : std::false_type {};

template<typename T>
struct CaptureSessionPauseSupported<
    T,
    decltype(wlr_ext_image_copy_capture_session_v1_set_paused(std::declval<T *>(), true),
             void())> : std::true_type {};

constexpr bool hasCaptureSessionPause =
    CaptureSessionPauseSupported<wlr_ext_image_copy_capture_session_v1>::value;

// Only instantiated when the API exists, so the call can't break older wlroots.
template<typename T = wlr_ext_image_copy_capture_session_v1>
void setCaptureSessionPaused(T *session, bool paused)
{
    wlr_ext_image_copy_capture_session_v1_set_paused(session, paused);
}

}

CaptureManager::CaptureManager(RootSurfaceContainer *root, QObject *parent)
    : QObject(parent)
    , m_rootSurfaceContainer(root)
{}

CaptureManager::~CaptureManager()
{
    qDeleteAll(m_captureSessions);
    m_captureSessions.clear();
    teardown();
}

void CaptureManager::installProtocol(WServer *server, QmlEngine *engine,
                                        WOutputRenderWindow *window)
{
    m_engine = engine;
    m_captureManagerV1 = server->attach<CaptureManagerV1>();
    m_captureManagerV1->setOutputRenderWindow(window);
    connect(m_captureManagerV1, &CaptureManagerV1::contextInSelectionChanged,
            this, &CaptureManager::onSelectionChanged);
}

void CaptureManager::installImageCapture(WServer *server, WExtForeignToplevelListV1 *toplevelList)
{
    m_extForeignToplevelListV1 = toplevelList;
    if (!wlr_screencopy_manager_v1_create(server->handle())) {
        qCCritical(lcTlCore) << "Failed to create screencopy manager";
    }

    m_imageCopyCaptureManager = wlr_ext_image_copy_capture_manager_v1_create(
        server->handle(), EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION);
    if (!m_imageCopyCaptureManager) {
        qCCritical(lcTlCore) << "Failed to create image copy capture manager";
    }

    if constexpr (hasCaptureSessionPause) {
        listeners()->add(&m_imageCopyCaptureManager->events.new_session,
                         this, &CaptureManager::handleNewImageCopyCaptureSession);
    } else {
        qCWarning(lcTlCapture) << "This wlroots lacks"
                               << "wlr_ext_image_copy_capture_session_v1_set_paused();"
                               << "capture sessions won't pause for inactive user sessions";
    }

    if (!wlr_ext_output_image_capture_source_manager_v1_create(
            server->handle(), EXT_OUTPUT_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION)) {
        qCCritical(lcTlCore) << "Failed to create output image capture source manager";
    }

    auto *manager = wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(
        server->handle(), EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION);
    if (manager) {
        listeners()->add(&manager->events.new_request,
                         this, &CaptureManager::handleNewForeignToplevelCaptureRequest);
    } else {
        qCCritical(lcTlCore) << "Failed to create foreign-toplevel image capture manager; capture disabled";
    }
}

void CaptureManager::onSelectionChanged()
{
    if (m_captureManagerV1->contextInSelection()) {
        m_captureSelector = qobject_cast<CaptureSourceSelector *>(
            m_engine->createCaptureSelector(m_rootSurfaceContainer, m_captureManagerV1));
    } else if (m_captureSelector) {
        m_captureSelector->deleteLater();
    }
}

// Returns the listening socket the capture client is connected to, or nullptr
// for compositor-internal and test clients. Sandboxed clients connect through
// a child socket, so walk up to the socket session switching toggles.
[[maybe_unused]] static WSocket *captureSessionSocket(
    wlr_ext_image_copy_capture_session_v1 *session)
{
    if (!session->resource)
        return nullptr;

    WClient *client = WClient::get(wl_resource_get_client(session->resource));
    if (!client || !client->socket())
        return nullptr;

    return client->socket()->rootSocket();
}

void CaptureManager::handleNewImageCopyCaptureSession(
    wlr_ext_image_copy_capture_session_v1 *session)
{
    if (!session)
        return;

    auto *entry = new CaptureSessionEntry;
    entry->session = session;
    entry->destroyListener.init(&session->events.destroy, [this, entry](void *) {
        m_captureSessions.removeOne(entry);
        delete entry;
    });
    m_captureSessions.append(entry);

    // waylib freezes the clients of a disabled socket; capture sessions follow
    // that same predicate so their frames freeze with their user.
    if (auto *socket = captureSessionSocket(session)) {
        connect(socket,
                &WSocket::enabledChanged,
                this,
                &CaptureManager::updatePausedCaptureSessions,
                Qt::UniqueConnection);
    }

    updatePausedCaptureSession(session);
}

void CaptureManager::updatePausedCaptureSession(
    wlr_ext_image_copy_capture_session_v1 *session)
{
    if constexpr (hasCaptureSessionPause) {
        WSocket *socket = captureSessionSocket(session);

        // Sessions without a known socket have no user session to freeze for:
        // keep them capturing instead of pausing them forever.
        setCaptureSessionPaused<>(session, socket && !socket->isEnabled());
    } else {
        Q_UNUSED(session)
    }
}

void CaptureManager::updatePausedCaptureSessions()
{
    const auto sessions = m_captureSessions;
    for (auto *entry : sessions)
        updatePausedCaptureSession(entry->session);
}

bool CaptureManager::isSelecting() const
{
    return m_captureSelector;
}

void CaptureManager::cancelSelection()
{
    if (m_captureSelector)
        m_captureSelector->cancelSelection();
}

void CaptureManager::handleNewForeignToplevelCaptureRequest(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request)
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
