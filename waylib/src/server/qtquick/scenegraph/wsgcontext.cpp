// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgcontext_p.h"
#include "wsgbatchrenderer_p.h"
#include "wayliblogging.h"

#include <private/qquickrendercontrol_p.h>
#include <private/qsgdefaultcontext_p.h>
#include <private/qsgdefaultrendercontext_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGRenderContext : public QSGDefaultRenderContext
{
public:
    using QSGDefaultRenderContext::QSGDefaultRenderContext;

    QSGRenderer *createRenderer(QSGRendererInterface::RenderMode renderMode = QSGRendererInterface::RenderMode2D) override
    {
        return WSGBatchRenderer::Renderer::create(this, renderMode);
    }
};

class WSGDefaultContext : public QSGDefaultContext
{
public:
    using QSGDefaultContext::QSGDefaultContext;

    QSGRenderContext *createRenderContext() override
    {
        return new WSGRenderContext(this);
    }
};

void WSGContext::ensureInstalled()
{
    static bool installed = false;
    if (installed)
        return;

    if (QQuickRenderControlPrivate::sg) {
        if (dynamic_cast<WSGDefaultContext *>(QQuickRenderControlPrivate::sg)) {
            installed = true;
            return;
        }
        qCCritical(lcWlRenderer) << "QQuickRenderControlPrivate::sg already exists and is not WSGContext";
        qFatal("WSGContext must be installed before any QQuickRenderControl is constructed");
    }

    QSGContext *sg = QSGContext::createDefaultContext();
    // QSGDefaultContext has no Q_OBJECT, so qobject_cast cannot identify it.
    if (dynamic_cast<QSGDefaultContext *>(sg)) {
        delete sg;
        sg = new WSGDefaultContext;
        qCDebug(lcWlRenderer) << "Installed WSGContext as QQuickRenderControl scene graph factory";
    } else {
        qCDebug(lcWlRenderer) << "Skip WSGContext; using Qt scene graph adaptation";
    }

    // Qt only registers this when it creates sg itself.
    qAddPostRoutine(QQuickRenderControlPrivate::cleanup);
    QQuickRenderControlPrivate::sg = sg;
    installed = true;
}

WAYLIB_SERVER_END_NAMESPACE
