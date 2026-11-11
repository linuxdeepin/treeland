// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "capturemanager.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "modules/capture/capture.h"
#include "session/session.h"
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

#define EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION 1
#define EXT_OUTPUT_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1
#define EXT_FOREIGN_TOPLEVEL_IMAGE_CAPTURE_SOURCE_MANAGER_V1_VERSION 1

CaptureManager::CaptureManager(RootSurfaceContainer *root, QObject *parent)
    : QObject(parent)
    , m_rootSurfaceContainer(root)
{}

CaptureManager::~CaptureManager()
{
    // The filter would otherwise keep pointing at this object after it is gone.
    if (m_screencopyManager)
        wlr_screencopy_manager_v1_set_capture_filter(m_screencopyManager, nullptr, nullptr);
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

void CaptureManager::installImageCapture(WServer *server, WExtForeignToplevelListV1 *toplevelList,
                                         SessionManager *sessionManager)
{
    m_extForeignToplevelListV1 = toplevelList;
    m_sessionManager = sessionManager;
    m_screencopyManager = wlr_screencopy_manager_v1_create(server->handle());
    if (!m_screencopyManager) {
        qCCritical(lcTlCore) << "Failed to create screencopy manager";
    } else if (m_sessionManager) {
        // Only the active session may see the screen: the filter keeps frames
        // of inactive sessions frozen until they are activated again.
        wlr_screencopy_manager_v1_set_capture_filter(m_screencopyManager,
                                                     &CaptureManager::canCaptureFrame,
                                                     this);
        connect(m_sessionManager, &SessionManager::sessionChanged,
                this, &CaptureManager::updateScreencopyPause);
        connect(m_sessionManager, &SessionManager::activeSessionRemoved,
                this, &CaptureManager::updateScreencopyPause);
    }

    if (!wlr_ext_image_copy_capture_manager_v1_create(server->handle(),
                                                   EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION)) {
        qCCritical(lcTlCore) << "Failed to create image copy capture manager";
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

bool CaptureManager::canCaptureFrame(wlr_screencopy_frame_v1 *frame, void *data)
{
    auto *self = static_cast<CaptureManager *>(data);
    wl_client *client = frame && frame->resource ? wl_resource_get_client(frame->resource)
                                                 : nullptr;
    return self->m_sessionManager
        && self->m_sessionManager->isActiveSessionClient(WClient::get(client));
}

void CaptureManager::updateScreencopyPause()
{
    if (!m_screencopyManager || !m_sessionManager)
        return;

    // The filter only decides about frames created from now on: pending frames
    // are still waiting for a commit, so they are frozen or resumed here.
    wlr_screencopy_frame_v1 *frame = nullptr;
    wl_list_for_each(frame, &m_screencopyManager->frames, link) {
        wl_client *client = wl_resource_get_client(frame->resource);
        const bool active = m_sessionManager->isActiveSessionClient(WClient::get(client));
        wlr_screencopy_frame_v1_set_paused(frame, !active);
    }
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
