// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wscoplistener.h>
#include <QObject>
#include <QPointer>

struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request;
class RootSurfaceContainer;
class QmlEngine;
class CaptureSourceSelector;
class CaptureManagerV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WExtForeignToplevelListV1;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class CaptureManager : public QObject, public WObject
{
    Q_OBJECT
public:
    CaptureManager(RootSurfaceContainer *root, QObject *parent = nullptr);
    ~CaptureManager() override;

    void installProtocol(WServer *server, QmlEngine *engine, WOutputRenderWindow *window);
    void installImageCapture(WServer *server, WExtForeignToplevelListV1 *toplevelList);
    bool isSelecting() const;
    void cancelSelection();

private:
    void onSelectionChanged();
    void handleNewForeignToplevelCaptureRequest(
        wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request);

private:
    RootSurfaceContainer *m_rootSurfaceContainer;
    QmlEngine *m_engine = nullptr;
    CaptureManagerV1 *m_captureManagerV1 = nullptr;
    WExtForeignToplevelListV1 *m_extForeignToplevelListV1 = nullptr;
    QPointer<CaptureSourceSelector> m_captureSelector;
};
