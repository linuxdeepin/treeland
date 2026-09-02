// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wserver.h"

WAYLIB_SERVER_USE_NAMESPACE

class ScreensaverInterfaceV2Private;
class ScreensaverInterfaceV2 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit ScreensaverInterfaceV2(QObject *parent = nullptr);
    ~ScreensaverInterfaceV2() override;

    QByteArrayView interfaceName() const override;
    bool isInhibited() const;

    static constexpr int InterfaceVersion = 1;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<ScreensaverInterfaceV2Private> d;
};
