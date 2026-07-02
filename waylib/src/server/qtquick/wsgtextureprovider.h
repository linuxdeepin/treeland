// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QSGTextureProvider>

QT_BEGIN_NAMESPACE
class QRhiCommandBuffer;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_texture;
class qw_buffer;
QW_END_NAMESPACE
struct wlr_surface;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputRenderWindow;
class WSGTextureProviderPrivate;
class WAYLIB_SERVER_EXPORT WSGTextureProvider : public QSGTextureProvider, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSGTextureProvider)

    Q_PROPERTY(bool smooth READ smooth WRITE setSmooth NOTIFY smoothChanged FINAL)

public:
    explicit WSGTextureProvider(WOutputRenderWindow *window);

    WOutputRenderWindow *window() const;
    static bool prefersDirectBufferImport(WOutputRenderWindow *window);

    bool directBufferImportAllowed() const;
    void setDirectBufferImportAllowed(bool allowed);

    bool setBuffer(QW_NAMESPACE::qw_buffer *buffer);
    bool setBuffer(QW_NAMESPACE::qw_buffer *buffer, wlr_surface *surface);
    bool setBuffer(QW_NAMESPACE::qw_buffer *buffer, wlr_surface *surface,
                   QRhiCommandBuffer *commandBuffer);
    bool setTexture(QW_NAMESPACE::qw_texture *texture, QW_NAMESPACE::qw_buffer *srcBuffer);
    bool setTexture(QW_NAMESPACE::qw_texture *texture, QW_NAMESPACE::qw_buffer *srcBuffer,
                    wlr_surface *surface);
    bool setTexture(QW_NAMESPACE::qw_texture *texture, QW_NAMESPACE::qw_buffer *srcBuffer,
                    wlr_surface *surface, QRhiCommandBuffer *commandBuffer);
    void invalidate();

    QSGTexture *texture() const override;
    virtual QW_NAMESPACE::qw_texture *qwTexture() const;
    virtual QW_NAMESPACE::qw_buffer *qwBuffer() const;

    bool smooth() const;
    void setSmooth(bool newSmooth);

Q_SIGNALS:
    void smoothChanged();
};

WAYLIB_SERVER_END_NAMESPACE
