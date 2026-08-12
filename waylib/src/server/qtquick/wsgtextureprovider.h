// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>

#include <QSGTextureProvider>

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

    void setBuffer(wlr_buffer *buffer);
    void setTexture(wlr_texture *texture, wlr_buffer *srcBuffer);
    void invalidate();

    QSGTexture *texture() const override;
    virtual wlr_texture *qwTexture() const;
    virtual wlr_buffer *qwBuffer() const;

    bool smooth() const;
    void setSmooth(bool newSmooth);

Q_SIGNALS:
    void smoothChanged();
};

WAYLIB_SERVER_END_NAMESPACE
