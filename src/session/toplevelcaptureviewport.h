// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutputviewport.h>
#include <wsurfaceitem.h>

#include <QSet>

class SurfaceWrapper;
Q_MOC_INCLUDE("surface/surfacewrapper.h")

WAYLIB_SERVER_USE_NAMESPACE

class ToplevelCaptureViewport final : public WOutputViewport
{
    Q_OBJECT
    Q_PROPERTY(SurfaceWrapper *sourceSurface READ sourceSurface CONSTANT)

public:
    ToplevelCaptureViewport(SurfaceWrapper *wrapper, WOutput *output);
    ~ToplevelCaptureViewport() override;
    SurfaceWrapper *sourceSurface() const;

private:
    void updateSurfaceGeometry();
    void updateSurfaceFlags();
    void updateDecoration();
    void updateGeometry();
    void checkContentChanges();
    bool hasDirtyContent(QQuickItem *item);
    void watchTextureSource(QQuickItem *item);
    void textureUpdated();

    SurfaceWrapper *const m_wrapper;
    WSurfaceItem *const m_surfaceItem;
    QPointer<QQuickItem> m_titleBar;
    QQuickItem *m_border = nullptr;
    QSet<QObject *> m_textureSources;
};
