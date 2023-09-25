// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>

struct wl_display;
struct wl_client;

WAYLIB_SERVER_BEGIN_NAMESPACE

typedef int SOCKET;
class WSocketPrivate;
class WAYLIB_SERVER_EXPORT WSocket : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSocket)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged FINAL)
    Q_PROPERTY(bool listening READ isListening NOTIFY listeningChanged FINAL)
    Q_PROPERTY(QString fullServerName READ fullServerName NOTIFY fullServerNameChanged FINAL)
    Q_PROPERTY(WSocket* parentSocket READ parentSocket CONSTANT)

public:
    explicit WSocket(bool freezeClientWhenDisable, WSocket *parentSocket = nullptr, QObject *parent = nullptr);

    static WSocket *get(const wl_client *client);

    WSocket *parentSocket() const;
    WSocket *rootSocket() const;

    bool isValid() const;
    void close();
    SOCKET socketFd() const;
    QString fullServerName() const;

    bool autoCreate(const QString &directory = QString());
    bool create(const QString &filePath);
    bool create(int fd, bool doListen);

    bool isListening() const;
    bool listen(struct wl_display *display);

    void addClient(wl_client *client);
    void removeClient(wl_client *client);
    QList<wl_client*> clients() const;

    bool isEnabled() const;
    void setEnabled(bool on);

Q_SIGNALS:
    void enabledChanged();
    void validChanged();
    void listeningChanged();
    void fullServerNameChanged();
    void clientsChanged();
};

WAYLIB_SERVER_END_NAMESPACE
