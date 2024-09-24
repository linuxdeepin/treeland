// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurface.h"
#include "private/wglobal_p.h"

#include <qwcompositor.h>
#include <QObject>
#include <QPointer>

struct wlr_surface;
struct wlr_subsurface;

QW_BEGIN_NAMESPACE
class qw_subsurface;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSurfacePrivate : public WWrapObjectPrivate {
public:
    WSurfacePrivate(WSurface *qq, QW_NAMESPACE::qw_surface *handle);
    ~WSurfacePrivate();

    WWRAP_HANDLE_FUNCTIONS(QW_NAMESPACE::qw_surface, wlr_surface)

    wl_client *waylandClient() const override;

    // begin slot function
    void on_commit();
    void on_client_commit();
    // end slot function

    void init();
    void connect();
    void instantRelease() override;    // release qwobject etc.
    void updateOutputs();
    void setPrimaryOutput(WOutput *output);
    void setBuffer(QW_NAMESPACE::qw_buffer *newBuffer);
    void updateBuffer();
    void updateBufferOffset();
    void updatePreferredBufferScale();
    void preferredBufferScaleChange();

    WSurface *ensureSubsurface(wlr_subsurface *subsurface);
    void setSubsurface(QW_NAMESPACE::qw_subsurface *newSubsurface);
    void setHasSubsurface(bool newHasSubsurface);
    void updateHasSubsurface();

    W_DECLARE_PUBLIC(WSurface)

    QPointer<QW_NAMESPACE::qw_subsurface> subsurface;
    bool hasSubsurface = false;
    bool isSubsurface = false;  // qpointer would be null due to qwsubsurface' destroy, cache here
    uint32_t preferredBufferScale = 1;
    uint32_t explicitPreferredBufferScale = 0;

    std::unique_ptr<QW_NAMESPACE::qw_buffer, QW_NAMESPACE::qw_buffer::unlocker> buffer;
    QVector<WOutput*> outputs;
    WOutput *primaryOutput = nullptr;
    QMetaObject::Connection frameDoneConnection;
    QPoint bufferOffset;
};

WAYLIB_SERVER_END_NAMESPACE
