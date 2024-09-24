// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WXdgDecorationManagerPrivate;
class WAYLIB_SERVER_EXPORT WXdgDecorationManager : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgDecorationManager)
    Q_PROPERTY(DecorationMode preferredMode READ preferredMode WRITE setPreferredMode NOTIFY preferredModeChanged)
    QML_NAMED_ELEMENT(XdgDecorationManager)
    QML_UNCREATABLE("Can't create in qml")

public:
    explicit WXdgDecorationManager();

    enum DecorationMode {
        Undefined,  // client not send any mode
        None,       // client send unset mode
        Client,
        Server,
    };
    Q_ENUM(DecorationMode)

    DecorationMode preferredMode() const;
    void setPreferredMode(DecorationMode mode);

    Q_INVOKABLE void setModeBySurface(WAYLIB_SERVER_NAMESPACE::WSurface *surface, DecorationMode mode);
    Q_INVOKABLE DecorationMode modeBySurface(WAYLIB_SERVER_NAMESPACE::WSurface* surface) const;

    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void surfaceModeChanged(WAYLIB_SERVER_NAMESPACE::WSurface *surface, DecorationMode mode);
    void preferredModeChanged(DecorationMode mode);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager*)
