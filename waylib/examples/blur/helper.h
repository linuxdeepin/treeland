// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WQuickOutputLayout;
class WCursor;
class WSeat;
class WBackend;
WAYLIB_SERVER_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_renderer;
class qw_allocator;
class qw_compositor;
QW_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class Q_DECL_HIDDEN Helper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WQmlCreator* outputCreator MEMBER m_outputCreator CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);

    inline WBackend *backend() const {
        return m_backend;
    }

private:
    WServer *m_server = nullptr;
    WQmlCreator *m_outputCreator = nullptr;

    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;
    qw_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat;
};
