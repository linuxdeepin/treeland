// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <QWindow>
#include <QOffscreenSurface>
#ifndef QT_NO_OPENGL
#include <QOpenGLContext>
#endif

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace QW {
class WAYLIB_SERVER_EXPORT Window : public QWindow
{
public:
    explicit Window(QWindow *parent = nullptr)
        : QWindow(parent)
    {
        setObjectName(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE));
    }
};

class WAYLIB_SERVER_EXPORT OffscreenSurface : public QOffscreenSurface
{
public:
    explicit OffscreenSurface(QScreen *screen = nullptr, QObject *parent = nullptr)
        : QOffscreenSurface(screen, parent)
    {
        setObjectName(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE));
    }
};

#ifndef QT_NO_OPENGL
class WAYLIB_SERVER_EXPORT OpenGLContext : public QOpenGLContext
{
public:
    explicit OpenGLContext(EGLDisplay egl, EGLContext context, QObject *parent = nullptr)
        : QOpenGLContext(parent)
        , m_eglDisplay(egl)
        , m_eglContext(context)
    {
        setObjectName(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE));
    }

    inline EGLDisplay eglDisplay() const {
        return m_eglDisplay;
    }

    inline EGLContext eglContext() const {
        return m_eglContext;
    }

private:
    EGLDisplay m_eglDisplay;
    EGLContext m_eglContext;
};
#endif

}

WAYLIB_SERVER_END_NAMESPACE
