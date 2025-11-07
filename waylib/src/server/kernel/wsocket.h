// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QQmlEngine>

struct wl_display;
struct wl_client;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSocket;
class WClientPrivate;
class WAYLIB_SERVER_EXPORT WClient : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WClient)
    Q_PROPERTY(QByteArray sandboxEngine READ sandboxEngine CONSTANT FINAL)
    Q_PROPERTY(QByteArray appId READ appId CONSTANT FINAL)
    Q_PROPERTY(QByteArray instanceId READ instanceId CONSTANT FINAL)
    // Using for QQmlListProperty
    QML_ANONYMOUS

public:
    WSocket *socket() const;
    wl_client *handle() const;

    struct Credentials {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    };

    [[nodiscard]] QSharedPointer<Credentials> credentials() const;
    [[nodiscard]] int pidFD() const;

    [[nodiscard]] static QSharedPointer<Credentials> getCredentials(const wl_client *client);
    static WClient *get(const wl_client *client);

    QByteArray sandboxEngine() const;
    QByteArray appId() const;
    QByteArray instanceId() const;

public Q_SLOTS:
    void freeze();
    void activate();

private:
    friend class WSocket;
    friend class WlClientDestroyListener;
    explicit WClient(wl_client *client, WSocket *socket, bool isWlClientOwned = true);
    ~WClient() = default;
    using QObject::deleteLater;
};

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
    Q_PROPERTY(WSocket* parentSocket READ parentSocket NOTIFY parentSocketChanged FINAL)
    Q_PROPERTY(WSocket* rootSocket READ rootSocket CONSTANT FINAL)

public:
    explicit WSocket(bool freezeClientWhenDisable, QObject *parent = nullptr);
    explicit WSocket(WSocket *parentSocket, QObject *parent = nullptr);
    ~WSocket();

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
    bool bind(int fd);

    bool isListening() const;
    bool listen(struct wl_display *display);

    WClient *addClient(int fd);
    WClient *addClient(wl_client *client, bool isWlClientOwned = true);
    bool removeClient(wl_client *client);
    bool removeClient(WClient *client);
    const QList<WClient *> &clients() const;

    bool isEnabled() const;
    void setEnabled(bool on);

Q_SIGNALS:
    void enabledChanged();
    void validChanged();
    void listeningChanged();
    void fullServerNameChanged();
    void clientsChanged();
    void clientAdded(WClient *client);
    void aboutToBeDestroyedClient(WClient *client);
    void parentSocketChanged();

private:
    explicit WSocket(const char *sandboxEngine,
                     const char *appId,
                     const char *instanceId,
                     WSocket *parentSocket, QObject *parent = nullptr);

    friend class WXWayland;
    friend void security_context_handle_commit(struct wl_client*,
                                               struct wl_resource*);
    void setParentSocket(WSocket *parent);
};

WAYLIB_SERVER_END_NAMESPACE
