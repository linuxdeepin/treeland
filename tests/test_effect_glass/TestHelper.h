// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QQmlEngine>
#include <QString>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WQuickOutputLayout;
class WCursor;
class WSeat;
class WBackend;
WAYLIB_SERVER_END_NAMESPACE

struct wlr_renderer;
struct wlr_allocator;
struct wlr_compositor;

WAYLIB_SERVER_USE_NAMESPACE

/// Sets up a minimal wayland server with a headless wlroots backend so the
/// test gets a real OpenGL context via Mesa llvmpipe — no display required.
class TestHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WQmlCreator *outputCreator MEMBER m_outputCreator CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit TestHelper(QObject *parent = nullptr);

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);
    bool usesSoftwareRenderer() const;

private:
    WServer *m_server = nullptr;
    WQmlCreator *m_outputCreator = nullptr;
    WBackend *m_backend = nullptr;
    wlr_renderer *m_renderer = nullptr;
    wlr_allocator *m_allocator = nullptr;
    wlr_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat;
};
