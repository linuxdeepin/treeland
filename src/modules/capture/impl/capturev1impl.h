// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-capture-unstable-v1-protocol.h"
#include "wrappointer.h"

#include <wglobal.h>
#include <wsurface.h>

#include <qwbuffer.h>

#include <QObject>

Q_MOC_INCLUDE(<wsurface.h>)
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
class WClient;
WAYLIB_SERVER_END_NAMESPACE

void handle_treeland_capture_context_v1_destroy([[maybe_unused]] wl_client *client,
                                                wl_resource *resource);

void handle_treeland_capture_context_v1_capture(wl_client *client,
                                                wl_resource *resource,
                                                uint32_t frame);
void handle_treeland_capture_context_v1_create_session(wl_client *client,
                                                       wl_resource *resource,
                                                       uint32_t session);
void handle_treeland_capture_context_v1_select_source(wl_client *client,
                                                      wl_resource *resource,
                                                      uint32_t source_hint,
                                                      uint32_t freeze,
                                                      uint32_t with_cursor,
                                                      wl_resource *mask);

struct treeland_capture_context_v1 : public QObject
{
    Q_OBJECT
public:
    struct wl_resource *resource{ nullptr };
    bool withCursor{ false };
    bool freeze{ false };
    uint32_t sourceHint{ 0 };
    WrapPointer<WAYLIB_SERVER_NAMESPACE::WSurface> mask{ nullptr };
    // mask should be created so there must exist a wsurface

    void sendSourceFailed(uint32_t reason);
    void sendSourceReady(QRect region, uint32_t source_type);
    void setResource(wl_client *client, wl_resource *resource);

Q_SIGNALS:
    void beforeDestroy();
    void newSession(treeland_capture_session_v1 *session);
    void selectSource();
    void capture(treeland_capture_frame_v1 *frame);
};

void handle_treeland_capture_frame_v1_destroy([[maybe_unused]] wl_client *client,
                                              wl_resource *resource);
void handle_treeland_capture_frame_v1_copy(wl_client *client,
                                           wl_resource *resource,
                                           wl_resource *buffer);

struct treeland_capture_frame_v1 : public QObject
{
    Q_OBJECT
public:
    wl_resource *resource{ nullptr };
    void setResource(wl_client *client, wl_resource *resource);
    void sendBuffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
    void sendBufferDone();
    void sendReady();
    void sendFailed();

Q_SIGNALS:
    void copy(QW_NAMESPACE::qw_buffer *buffer);
    void beforeDestroy();
};

void handle_treeland_capture_session_v1_destroy(struct wl_client *client,
                                                struct wl_resource *resource);
void handle_treeland_capture_session_v1_start([[maybe_unused]] struct wl_client *client,
                                              struct wl_resource *resource);
void handle_treeland_capture_session_v1_frame_done(wl_client *client,
                                                   wl_resource *resource,
                                                   uint32_t tv_sec_hi,
                                                   uint32_t tv_sec_lo,
                                                   uint32_t tv_usec);

struct treeland_capture_session_v1 : public QObject
{
    Q_OBJECT
public:
    wl_resource *resource{ nullptr };

    void setResource(wl_client *client, wl_resource *resource);
    void sendProduceMoreCancel();
    void sendSourceDestroyCancel();
    void sendSourceResizeCancel();

Q_SIGNALS:
    void beforeDestroy();
    void start();
    void frameDone(uint32_t tvSecHi, uint32_t tvSecLo, uint32_t tvUsec);
};

void handle_treeland_capture_manager_v1_destroy([[maybe_unused]] wl_client *client,
                                                wl_resource *resource);
void handle_treeland_capture_manager_v1_get_context(wl_client *client,
                                                    wl_resource *resource,
                                                    uint32_t context);

struct treeland_capture_manager_v1 : public QObject
{
    Q_OBJECT
public:
    wl_global *global;
    QList<QPair<WAYLIB_SERVER_NAMESPACE::WClient *, wl_resource *>> clientResources;
    explicit treeland_capture_manager_v1(wl_display *display, QObject *parent = nullptr);
    void addClientResource(wl_client *client, wl_resource *resource);
Q_SIGNALS:
    void newCaptureContext(treeland_capture_context_v1 *context);
};
