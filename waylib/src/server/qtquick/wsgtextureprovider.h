// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QSGTextureProvider>

QW_BEGIN_NAMESPACE
class qw_texture;
class qw_buffer;
QW_END_NAMESPACE
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

    void setBuffer(QW_NAMESPACE::qw_buffer *buffer);
    void setTexture(QW_NAMESPACE::qw_texture *texture, QW_NAMESPACE::qw_buffer *srcBuffer);
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
