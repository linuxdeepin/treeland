// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <woutput.h>
#include <qwglobal.h>

#include <QObject>
#include <QQuickRenderTarget>
#include <QSGRendererInterface>

QT_BEGIN_NAMESPACE
class QOpenGLContext;
class QWindow;
class QQuickRenderControl;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class QWRenderer;
class QWBackend;
class qw_buffer;
QW_END_NAMESPACE

struct wlr_swapchain;
struct pixman_region32;
struct wlr_output_layer_state;
typedef QVarLengthArray<wlr_output_layer_state> wlr_output_layer_state_array;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputHelperPrivate;
class WAYLIB_SERVER_EXPORT WOutputHelper : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputHelper)
    Q_PROPERTY(bool contentIsDirty READ contentIsDirty NOTIFY contentIsDirtyChanged)
    Q_PROPERTY(bool needsFrame READ needsFrame FINAL)

public:
    explicit WOutputHelper(WOutput *output, QObject *parent = nullptr);

    WOutput *output() const;
    QWindow *outputWindow() const;

    std::pair<QW_NAMESPACE::qw_buffer*, QQuickRenderTarget> acquireRenderTarget(QQuickRenderControl *rc,
                                                                               wlr_swapchain **swapchain = nullptr);
    std::pair<QW_NAMESPACE::qw_buffer*, QQuickRenderTarget> lastRenderTarget();

    void setBuffer(QW_NAMESPACE::qw_buffer *buffer);
    QW_NAMESPACE::qw_buffer *buffer() const;

    void setScale(float scale);
    void setTransform(WOutput::Transform t);
    void setDamage(const pixman_region32 *damage);
    const pixman_region32 *damage() const;
    void setLayers(const wlr_output_layer_state_array &layers);
    bool commit();
    bool testCommit();
    bool testCommit(QW_NAMESPACE::qw_buffer *buffer, const wlr_output_layer_state_array &layers);

    bool contentIsDirty() const;
    bool needsFrame() const;
    bool framePending() const;

    void resetState();
    void update();
    void scheduleFrame();

protected:
    WOutputHelper(WOutput *output, bool contentIsDirty, QObject *parent = nullptr);

Q_SIGNALS:
    void contentIsDirtyChanged();
};

WAYLIB_SERVER_END_NAMESPACE
