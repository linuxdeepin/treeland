// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wsgtextureprovider.h>
WAYLIB_SERVER_BEGIN_NAMESPACE
class WTextureCapturerPrivate;

class WAYLIB_SERVER_EXPORT WTextureProviderProvider
{
public:
    virtual WOutputRenderWindow *outputRenderWindow() const = 0;
    virtual WSGTextureProvider *wTextureProvider() const = 0;
};

class WAYLIB_SERVER_EXPORT WTextureCapturer: public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WTextureCapturer)
public:
    explicit WTextureCapturer(WTextureProviderProvider *provider, QObject *parent = nullptr);
    QFuture<QImage> grabToImage();

private:
    void doGrabToImage();
};

WAYLIB_SERVER_END_NAMESPACE
