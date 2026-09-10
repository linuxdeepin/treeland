// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wxdgsurface.h>

#include <QObject>
#include <QPointer>
#include <QQmlEngine>

#include "common/windowdecorations.h"

#include <memory>

class SurfaceWrapper;
class DecorationManagerInterfaceV1;
class DecorationContextV1Private;

WAYLIB_SERVER_USE_NAMESPACE

/**
 * @brief Per-window SSD (server-side decoration) customization context.
 *
 * Wraps a treeland_decoration_context_v1 resource and exposes the
 * corner-radius, shadow, border, and titlebar-visibility overrides
 * requested by the client for a single surface.
 */
class DecorationContextV1 : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(uint32_t cornerRadius READ cornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(Shadow shadow READ shadow NOTIFY shadowChanged)
    Q_PROPERTY(Border border READ border NOTIFY borderChanged)
    Q_PROPERTY(bool noTitlebar READ noTitlebar NOTIFY titlebarModeChanged)

public:
    ~DecorationContextV1() override;

    wl_resource *resource() const;
    wlr_surface *surface() const;

    uint32_t cornerRadius() const;
    Shadow shadow() const;
    Border border() const;
    bool noTitlebar() const;

    static DecorationContextV1 *get(wl_resource *resource);
    static DecorationContextV1 *getContext(WSurface *surface);

Q_SIGNALS:
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void titlebarModeChanged();

private:
    explicit DecorationContextV1(wl_resource *resource, wlr_surface *surface);

    std::unique_ptr<DecorationContextV1Private> d;

    friend class DecorationManagerInterfaceV1Private;
    friend class DecorationContextV1Private;
};

/**
 * @brief Server implementation of treeland_decoration_unstable_v1.
 *
 * Global factory that creates per-surface DecorationContextV1 objects.
 */
class DecorationManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit DecorationManagerInterfaceV1(QObject *parent = nullptr);
    ~DecorationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void contextCreated(DecorationContextV1 *context);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
