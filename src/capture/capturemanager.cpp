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
