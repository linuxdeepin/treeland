// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE
class QSGNode;
class QSGTexture;
QT_END_NAMESPACE

namespace WSGBatchRenderer {

using TextureDrawCallback = void (*)(void *userData);

struct TextureDrawCallbackEntry
{
    const QSGTexture *texture = nullptr;
    TextureDrawCallback callback = nullptr;
    void *userData = nullptr;
};

// Registers a callback on a specific image node. The batch renderer invokes it
// after the batch containing that node has been drawn, provided the node still
// uses the registered texture. Keying by node (not by texture alone) keeps
// other image nodes, such as texture proxies, from reporting the surface as
// drawn. The callback must not add or remove scene graph nodes and may run on
// the render thread.
void registerNodeDrawCallback(QSGNode *node,
                              QSGTexture *texture,
                              TextureDrawCallback callback,
                              void *userData);
void unregisterNodeDrawCallback(QSGNode *node, const void *userData);
bool hasNodeDrawCallbacks();
TextureDrawCallbackEntry nodeDrawCallbackFor(const QSGNode *node);

} // namespace WSGBatchRenderer
