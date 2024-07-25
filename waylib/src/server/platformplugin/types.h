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
    static constexpr QAnyStringView id() {
        return "QWOutputWindow";
    }

    static bool check(QObject *obj) {
        return obj->objectName() == id();
    }

    explicit Window(QWindow *parent = nullptr)
        : QWindow(parent)
    {
        setObjectName(id());
    }
};

class Q_DECL_HIDDEN RenderWindow : public QWindow
{
public:
    static constexpr QAnyStringView id() {
        return "QWRenderWindow";
    }

    static bool check(QObject *obj) {
        return obj->objectName() == id();
    }

    explicit RenderWindow(QWindow *parent = nullptr)
        : QWindow(parent)
    {
        setObjectName(id());
    }

    static bool beforeDisposeEventFilter(QWindow *window, QEvent *event);
    static bool afterDisposeEventFilter(QWindow *window, QEvent *event);
};

class Q_DECL_HIDDEN OffscreenSurface : public QOffscreenSurface
{
public:
    static constexpr QAnyStringView id() {
        return "QWOffscreenSurface";
    }

    static bool check(QObject *obj) {
        return obj->objectName() == id();
    }

    explicit OffscreenSurface(QScreen *screen = nullptr, QObject *parent = nullptr)
        : QOffscreenSurface(screen, parent)
    {
        setObjectName(id());
    }
};

#ifndef QT_NO_OPENGL
class Q_DECL_HIDDEN OpenGLContext : public QOpenGLContext
{
    Q_OBJECT
public:
    static constexpr QAnyStringView id() {
        return "QWOpenGLContext";
    }

    static bool check(QObject *obj) {
        return obj->objectName() == id();
    }

    explicit OpenGLContext(EGLDisplay egl, EGLContext context, QObject *parent = nullptr)
        : QOpenGLContext(parent)
        , m_eglDisplay(egl)
        , m_eglContext(context)
    {
        setObjectName(id());
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
