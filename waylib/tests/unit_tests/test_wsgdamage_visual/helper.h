// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>

Q_MOC_INCLUDE(<wcursor.h>)
Q_MOC_INCLUDE(<wquickoutputlayout.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WSocket;
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

class VisualHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WQmlCreator *outputCreator MEMBER m_outputCreator CONSTANT)
    Q_PROPERTY(WQmlCreator *xdgShellCreator MEMBER m_xdgShellCreator CONSTANT)
    Q_PROPERTY(WQuickOutputLayout *outputLayout READ outputLayout CONSTANT)
    Q_PROPERTY(WCursor *cursor READ cursor CONSTANT)
    QML_NAMED_ELEMENT(Helper)
    QML_SINGLETON

public:
    explicit VisualHelper(QObject *parent = nullptr);

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);
    wlr_renderer *renderer() const { return m_renderer; }
    WQuickOutputLayout *outputLayout() const { return m_outputLayout; }
    WCursor *cursor() const { return m_cursor; }
    QString waylandSocketName() const;
    int createInProcessClientFd();
    void dispatchWaylandEvents();

private:
    WServer *m_server = nullptr;
    WSocket *m_socket = nullptr;
    WQmlCreator *m_outputCreator = nullptr;
    WQmlCreator *m_xdgShellCreator = nullptr;
    WBackend *m_backend = nullptr;
    wlr_renderer *m_renderer = nullptr;
    wlr_allocator *m_allocator = nullptr;
    wlr_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat;
};
