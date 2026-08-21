// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>

#include <QObject>
#include <QPointer>
#include <QQmlEngine>

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

class Q_DECL_HIDDEN Helper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WQmlCreator *outputCreator MEMBER m_outputCreator CONSTANT)
    Q_PROPERTY(QString damageDebugMode READ damageDebugMode WRITE setDamageDebugMode NOTIFY damageDebugModeChanged)
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helper(QObject *parent = nullptr);

    void initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine);

    inline WBackend *backend() const
    {
        return m_backend;
    }

    QString damageDebugMode() const;
    void setDamageDebugMode(const QString &mode);

Q_SIGNALS:
    void damageDebugModeChanged();

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
