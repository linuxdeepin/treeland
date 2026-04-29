// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wserver.h"

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class DDMInterfaceV1Private;
class DDMInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit DDMInterfaceV1(QObject *parent = nullptr);
    ~DDMInterfaceV1() override;

    bool isConnected() const;
    void switchToVt(const int vtnr);
    void acquireVt(const int vtnr);

    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 1;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<DDMInterfaceV1Private> d;
};
