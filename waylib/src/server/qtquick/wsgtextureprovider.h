// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>

#include <QSGTextureProvider>

QT_BEGIN_NAMESPACE
class QSGNode;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputRenderWindow;
class WSurfaceItemContent;
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
    bool hasTexture() const;
    virtual wlr_texture *qwTexture() const;
    virtual wlr_buffer *wlrBuffer() const;

    // Registers the owning surface item and its image node so the vendored
    // batch renderer can report a "drawn" event after that node is rendered.
    // This is the Vulkan presentation/VKTRACE path; GLES2 keeps its render
    // node.
    void setSurfaceContent(WSurfaceItemContent *content, QSGNode *imageNode);
    void clearSurfaceContent();

    bool smooth() const;
    void setSmooth(bool newSmooth);

private:
    static void textureDrawCallback(void *userData);
    void notifyTextureDrawn();

Q_SIGNALS:
    void smoothChanged();
};

WAYLIB_SERVER_END_NAMESPACE
