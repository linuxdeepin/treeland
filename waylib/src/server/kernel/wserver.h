// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwglobal.h>

#include <QDeadlineTimer>
#include <QFuture>
#include <QObject>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

QW_BEGIN_NAMESPACE
class qw_display;
QW_END_NAMESPACE

struct wl_global;

WAYLIB_SERVER_BEGIN_NAMESPACE

typedef bool (*GlobalFilterFunc)(const wl_client *client,
                                 const wl_global *global,
                                 void *data);

class WServer;
class WSocket;
class WClient;
class WAYLIB_SERVER_EXPORT WServerInterface
{
public:
    virtual ~WServerInterface() {}
    inline void *handle() const {
        return m_handle;
    }
    inline bool isValid() const {
        return m_handle;
    }

    template<typename DNativeInterface>
    DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    inline WServer *server() const {
        return m_server;
    }

    inline std::function<bool(WClient*)> filter() const {
        return m_filter;
    }

    inline void setFilter(std::function<bool(WClient*)> f) {
        m_filter = f;
    }

    virtual QByteArrayView interfaceName() const = 0;

protected:
    void *m_handle = nullptr;
    WServer *m_server = nullptr;
    std::function<bool(WClient*)> m_filter;

    virtual void create(WServer *server) {
        Q_UNUSED(server);
    }
    virtual void destroy(WServer *server) {
        Q_UNUSED(server);
    }
    virtual wl_global *global() const = 0;

    friend class WServer;
    friend class WServerPrivate;
};

class WSocket;
class WServerPrivate;
class WAYLIB_SERVER_EXPORT WServer : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WServer)
    friend class WShellInterface;

public:
    explicit WServer(QObject *parent = nullptr);
    ~WServer();

    QW_NAMESPACE::qw_display *handle() const;

    void attach(WServerInterface *interface);
    template<typename Interface, typename... Args>
    Interface *attach(Args&&...args) {
        static_assert(std::is_base_of<WServerInterface, Interface>::value,
                "Not base of WServerInterface");
        auto interface = new Interface(std::forward<Args>(args)...);
        attach(interface);
        return interface;
    }
    template<typename Interface>
    Interface *attach() {
        static_assert(std::is_base_of<WServerInterface, Interface>::value,
                "Not base of WServerInterface");
        auto interface = new Interface();
        attach(interface);
        return interface;
    }
    bool detach(WServerInterface *interface);

    const QVector<WServerInterface *> &interfaceList() const;
    QVector<WServerInterface*> findInterfaces(void *handle) const;
    WServerInterface *findInterface(void *handle) const;
    WServerInterface *findInterface(const wl_global *global) const;
    template<typename Interface>
    QVector<Interface*> findInterfaces() const {
        QVector<Interface*> list;
        for (auto i : interfaceList()) {
            if (auto ii = dynamic_cast<Interface*>(i))
                list << ii;
        }

        return list;
    }
    template<typename Interface>
    Interface *findInterface() const {
        for (auto i : interfaceList()) {
            if (auto ii = dynamic_cast<Interface*>(i))
                return ii;
        }

        return nullptr;
    }

    static WServer *from(WServerInterface *interface);

    void start();
    void stop();
    static void initializeQPA(bool master = true, const QStringList &parameters = {});
    void initializeProxyQPA(int &argc, char **argv, const QStringList &proxyPlatformPlugins = {}, const QStringList &parameters = {});

    bool isRunning() const;
    void addSocket(WSocket *socket);

    void setGlobalFilter(GlobalFilterFunc filter, void *data);

Q_SIGNALS:
    void started();

protected:
    WServer(WServerPrivate &dd, QObject *parent = nullptr);
};

WAYLIB_SERVER_END_NAMESPACE
