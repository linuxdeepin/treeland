// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QObject>

#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

class XdgDialogManagerInterfaceV1Private;

class XdgDialogManagerInterfaceV1 : public QObject, public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit XdgDialogManagerInterfaceV1(QObject *parent = nullptr);
    ~XdgDialogManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void surfaceModalChanged(WAYLIB_SERVER_NAMESPACE::WSurface *surface, bool modal);

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<XdgDialogManagerInterfaceV1Private> d;
};
