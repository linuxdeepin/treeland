// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WOutputRenderWindow;
class WQuickOutputLayout;
class WCursor;
class WSeat;
class WBackend;
class WOutputItem;
class WOutputViewport;
class WOutputLayer;
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
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);
    ~Helper();

    void initProtocols(QQmlEngine *qmlEngine);

    inline WBackend *backend() const {
        return m_backend;
    }

private:
    void updatePrimaryOutputHardwareLayers();

    WServer *m_server = nullptr;

    WBackend *m_backend = nullptr;
    qw_renderer *m_renderer = nullptr;
    qw_allocator *m_allocator = nullptr;
    qw_compositor *m_compositor = nullptr;
    WQuickOutputLayout *m_outputLayout = nullptr;
    WCursor *m_cursor = nullptr;
    QPointer<WSeat> m_seat;

    WOutputRenderWindow *m_renderWindow = nullptr;
    WOutputItem *m_primaryOutput = nullptr;
    QPointer<WOutputViewport> m_primaryOutputViewport;
    QList<WOutputLayer*> m_hardwareLayersOfPrimaryOutput;
    QList<std::pair<WOutputViewport*, QQuickItem*>> m_copyOutputs;
};
