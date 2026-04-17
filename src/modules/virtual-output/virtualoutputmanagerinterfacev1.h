// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class VirtualOutputInterfaceV1;
class VirtualOutputManagerInterfaceV1Private;
class VirtualOutputManagerInterfaceV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT

public:
    explicit VirtualOutputManagerInterfaceV1(QObject *parent = nullptr);
    ~VirtualOutputManagerInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;
Q_SIGNALS:
    void requestCreateVirtualOutput(VirtualOutputInterfaceV1 *interface);
    void destroyVirtualOutput(VirtualOutputInterfaceV1 *interface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    friend class VirtualOutputManagerInterfaceV1Private;
    std::unique_ptr<VirtualOutputManagerInterfaceV1Private> d;
};

class VirtualOutputInterfaceV1Private;
class VirtualOutputInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    enum ERROR {
        INVALID_GROUP_NAME = 0,
        INVALID_SCREEN_NUMBER = 1,
        INVALID_OUTPUT = 2,
    };
    ~VirtualOutputInterfaceV1() override;

    wl_resource *resource() const;
    QStringList outputList() const;
    void sendOutputs(const QString &name, const QByteArray &outputs);
    void sendError(uint32_t code, const QString &message);
    static VirtualOutputInterfaceV1 *get(wl_resource *resource);

Q_SIGNALS:
    void beforeDestroy(VirtualOutputInterfaceV1 *ouput);

private:
    explicit VirtualOutputInterfaceV1(const QString &name,
                                      wl_array *outputs,
                                      wl_resource *resource);

private:
    friend class VirtualOutputManagerInterfaceV1Private;
    std::unique_ptr<VirtualOutputInterfaceV1Private> d;
};
