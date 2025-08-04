// Copyright (C) 2025 April Lu <apr3vau@outlook.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wserver.h"

/**
 * The server-side wrapper for treeland private protocol.
 */
class DDMInterfaceV1 : public Waylib::Server::WServerInterface {
public:
    DDMInterfaceV1();
    ~DDMInterfaceV1() override;
    QByteArrayView interfaceName() const override;
    bool isConnected() const;
    void switchToVt(int vtnr);
    void acquireVt(int vtnr);
protected:
    void create(Waylib::Server::WServer *server) override;
    void destroy(Waylib::Server::WServer *server) override;
    wl_global *global() const override;
private:
    struct wl_global *m_global { nullptr };
};
