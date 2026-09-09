// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearanceinterfacev1.h"
#include "appearancemanagerinterfacev1.h"

#include "qwayland-server-treeland-appearance-unstable-v1.h"

#include "common/treelandlogging.h"
#include "seat/helper.h"
#include "treelanduserconfig.hpp"

#include <wayland-server-core.h>

#include <QColor>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// DConfig windowThemeType (1=light, 2=dark) → protocol color_scheme (0=light, 1=dark)
uint32_t dconfigToColorScheme(int32_t type)
{
    switch (type) {
    case 2: return TREELAND_APPEARANCE_V1_COLOR_SCHEME_DARK;
    case 1: return TREELAND_APPEARANCE_V1_COLOR_SCHEME_LIGHT;
    default:
        qCWarning(lcTlConfig) << "Unknown dconfig windowThemeType:" << type
                              << ", fallback to light.";
        return TREELAND_APPEARANCE_V1_COLOR_SCHEME_LIGHT;
    }
}

/// Parse "#RRGGBB" hex string to RGBA components (alpha = 255).
struct Rgba { uint32_t r, g, b, a; };
Rgba hexToRgba(const QString &hex)
{
    QColor c(hex);
    if (!c.isValid()) {
        qCWarning(lcTlConfig) << "Invalid color string:" << hex;
        return {0, 0, 0, 255};
    }
    return {static_cast<uint32_t>(c.red()),
            static_cast<uint32_t>(c.green()),
            static_cast<uint32_t>(c.blue()),
            static_cast<uint32_t>(c.alpha())};
}

} // namespace

// ---------------------------------------------------------------------------
// AppearanceInterfaceV1Private
// ---------------------------------------------------------------------------

class AppearanceInterfaceV1Private
    : public QtWaylandServer::treeland_appearance_v1
{
public:
    explicit AppearanceInterfaceV1Private(AppearanceInterfaceV1 *_q);
    ~AppearanceInterfaceV1Private() override = default;

    wl_global *global() const { return m_global; }

    AppearanceInterfaceV1 *q = nullptr;

protected:
    void destroy(Resource *resource) override;
    void bind_resource(Resource *resource) override;
};

AppearanceInterfaceV1Private::AppearanceInterfaceV1Private(AppearanceInterfaceV1 *_q)
    : QtWaylandServer::treeland_appearance_v1()
    , q(_q)
{
}

void AppearanceInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AppearanceInterfaceV1Private::bind_resource(Resource *resource)
{
    // Push model: send the current value of every setting immediately on bind.
    q->sendAll(resource->handle);
}

// ---------------------------------------------------------------------------
// AppearanceInterfaceV1
// ---------------------------------------------------------------------------

struct AppearanceInterfaceV1::Private
{
    std::unique_ptr<AppearanceInterfaceV1Private> server;
    QList<QMetaObject::Connection> configConnections;
};

AppearanceInterfaceV1::AppearanceInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<Private>())
{
    d->server = std::make_unique<AppearanceInterfaceV1Private>(this);

    // The TreelandUserConfig object is replaced in Helper::init when the
    // current user changes. Reconnect to the new config's signals whenever
    // that happens, and push the new values to all bound clients.
    connect(Helper::instance(), &Helper::configChanged, this, [this] {
        setupConfigConnections();
        // Push the new config values to all already-bound clients.
        for (const auto &resource : d->server->resourceMap())
            sendAll(resource->handle);
    });

    // Also set up connections for the initial config (available in the
    // Helper constructor, though it may be replaced later).
    setupConfigConnections();
}

AppearanceInterfaceV1::~AppearanceInterfaceV1()
{
    for (const auto &conn : std::as_const(d->configConnections))
        QObject::disconnect(conn);
}

void AppearanceInterfaceV1::setupConfigConnections()
{
    // Disconnect from the previous config object's signals.
    for (const auto &conn : std::as_const(d->configConnections))
        QObject::disconnect(conn);
    d->configConnections.clear();

    auto *config = Helper::instance()->config();
    if (!config)
        return;

    d->configConnections.append(
        connect(config, &TreelandUserConfig::cursorThemeNameChanged, this, [this] {
            const auto name = Helper::instance()->config()->cursorThemeName();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_cursor_theme(resource->handle, name);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::cursorSizeChanged, this, [this] {
            const auto size = Helper::instance()->config()->cursorSize();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_cursor_size(resource->handle, size, size);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::fontChanged, this, [this] {
            const auto font = Helper::instance()->config()->font();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_font(resource->handle, font);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::monoFontChanged, this, [this] {
            const auto font = Helper::instance()->config()->monoFont();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_monospace_font(resource->handle, font);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::fontSizeChanged, this, [this] {
            const auto size = static_cast<uint32_t>(Helper::instance()->config()->fontSize());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_font_size(resource->handle, size);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::iconThemeNameChanged, this, [this] {
            const auto theme = Helper::instance()->config()->iconThemeName();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_icon_theme(resource->handle, theme);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::activeColorChanged, this, [this] {
            const auto rgba = hexToRgba(Helper::instance()->config()->activeColor());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_accent_color(resource->handle, rgba.r, rgba.g, rgba.b, rgba.a);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowOpacityChanged, this, [this] {
            const auto opacity = wl_fixed_from_double(
                Helper::instance()->config()->windowOpacity() / 100.0);
            for (const auto &resource : d->server->resourceMap())
                d->server->send_window_opacity(resource->handle, opacity);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowThemeTypeChanged, this, [this] {
            const auto scheme = dconfigToColorScheme(
                Helper::instance()->config()->windowThemeType());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_color_scheme(resource->handle, scheme);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowTitlebarHeightChanged, this, [this] {
            const auto height = static_cast<uint32_t>(
                Helper::instance()->config()->windowTitlebarHeight());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_titlebar_height(resource->handle, height);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowRadiusChanged, this, [this] {
            const auto radius = static_cast<uint32_t>(
                Helper::instance()->config()->windowRadius());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_corner_radius(resource->handle, radius);
        }));
}

void AppearanceInterfaceV1::sendAll(wl_resource *resource)
{
    auto *config = Helper::instance()->config();
    if (!config)
        return;

    d->server->send_cursor_theme(resource, config->cursorThemeName());
    const int cursorSize = config->cursorSize();
    d->server->send_cursor_size(resource, cursorSize, cursorSize);
    d->server->send_font(resource, config->font());
    d->server->send_monospace_font(resource, config->monoFont());
    d->server->send_font_size(resource, static_cast<uint32_t>(config->fontSize()));
    d->server->send_icon_theme(resource, config->iconThemeName());

    const auto rgba = hexToRgba(config->activeColor());
    d->server->send_accent_color(resource, rgba.r, rgba.g, rgba.b, rgba.a);

    d->server->send_window_opacity(resource,
        wl_fixed_from_double(config->windowOpacity() / 100.0));

    d->server->send_color_scheme(resource, dconfigToColorScheme(config->windowThemeType()));
    d->server->send_titlebar_height(resource,
        static_cast<uint32_t>(config->windowTitlebarHeight()));
    d->server->send_corner_radius(resource,
        static_cast<uint32_t>(config->windowRadius()));
}

QByteArrayView AppearanceInterfaceV1::interfaceName() const
{
    return d->server->interfaceName();
}

void AppearanceInterfaceV1::create(WServer *server)
{
    d->server->init(server->handle(), InterfaceVersion);
}

void AppearanceInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->server->globalRemove();
}

wl_global *AppearanceInterfaceV1::global() const
{
    return d->server->global();
}
