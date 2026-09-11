// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wsurface.h>

#include <QObject>
#include <QSize>

WAYLIB_SERVER_USE_NAMESPACE

class LayerShellExtensionManagerInterfaceV1Private;
class LayerShellExtensionObjectV1Private;
class LayerShellExtensionObjectV1;

class LayerShellExtensionManagerInterfaceV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit LayerShellExtensionManagerInterfaceV1(QObject *parent = nullptr);
    ~LayerShellExtensionManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;
    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void objectCreated(LayerShellExtensionObjectV1 *interface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<LayerShellExtensionManagerInterfaceV1Private> d;
};

class LayerShellExtensionObjectV1 : public QObject
{
    Q_OBJECT
public:
    ~LayerShellExtensionObjectV1() override;

    WSurface *wSurface() const;
    wl_resource *nativeSurface() const;

    void sendResizing(uint32_t resizing);
    void endResize();

Q_SIGNALS:
    void beforeDestroy();

private:
    explicit LayerShellExtensionObjectV1(wl_resource *surface, wl_resource *resource);

private:
    friend class LayerShellExtensionManagerInterfaceV1Private;
    std::unique_ptr<LayerShellExtensionObjectV1Private> d;
};
