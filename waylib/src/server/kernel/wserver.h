/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WServerInterface
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

protected:
    void *m_handle = nullptr;

    virtual void create(WServer *server) = 0;
    virtual void destroy(WServer *server) = 0;
    friend class WServer;
    friend class WServerPrivate;
};

class WThreadUtil;
class WInputDevice;
class WOutput;
class WOutputHandle;
class WDisplayHandle;
class WBackend;
class WBackendHandle;
class WServerPrivate;
class WServer : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WServer)
    friend class WShellInterface;

public:
    explicit WServer();

    WDisplayHandle *handle() const;
    template<typename WDisplayNativeInterface>
    WDisplayNativeInterface *nativeInterface() const {
        return reinterpret_cast<WDisplayNativeInterface*>(handle());
    }

    void attach(WServerInterface *interface);
    template<typename Interface, typename... Args>
    Interface *attach(Args&&...args) {
        static_assert(std::is_base_of<WServerInterface, Interface>::value,
                "Not base of WServerInterface");
        auto interface = new Interface(std::forward<Args...>(args...));
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

    QVector<WServerInterface*> interfaceList() const;
    QVector<WServerInterface*> findInterfaces(void *handle) const;
    WServerInterface *findInterface(void *handle) const;
    template<typename Interface>
    QVector<Interface*> findInterfaces() const {
        QVector<Interface*> list;
        Q_FOREACH(auto i, interfaceList()) {
            if (auto ii = dynamic_cast<Interface*>(i))
                list << ii;
        }

        return list;
    }
    template<typename Interface>
    Interface *findInterface() const {
        Q_FOREACH(auto i, interfaceList()) {
            if (auto ii = dynamic_cast<Interface*>(i))
                return ii;
        }

        return nullptr;
    }

    static WServer *fromThread(const QThread *thread);

    void start();
    void stop();

    bool waitForStarted(int timeout = -1);
    bool waitForStoped(int timeout = -1);
    bool isRunning() const;
    const char *displayName() const;

    WThreadUtil *threadUtil() const;

Q_SIGNALS:
    void started();
    // TODO: remove it, use the WBackend direct attach to WSeat
    void inputAdded(WBackend *backend, WInputDevice *input);
    void inputRemoved(WBackend *backend, WInputDevice *input);

protected:
    bool event(QEvent *e) override;
};

WAYLIB_SERVER_END_NAMESPACE
