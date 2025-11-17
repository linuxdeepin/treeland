// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wserver.h"

/**
 * Simple idle inhibit protocol.
 */
class ScreensaverInterfaceV1 : public Waylib::Server::WServerInterface {
public:
    QByteArrayView interfaceName() const override;
    void inhibit(wl_resource *res, const char *appName, const char *reason);
    uint32_t uninhibit(wl_resource *res);
    inline bool isInhibited() const { return !m_inhibits.isEmpty(); }
protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    inline wl_global *global() const override { return m_global; }
private:
    struct wl_global *m_global { nullptr };
    QHash<wl_resource*, std::tuple<const char*, const char*>> m_inhibits;
};
