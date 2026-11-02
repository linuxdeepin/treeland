// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearanceinterfacev1.h"

#include "qwayland-server-treeland-appearance-unstable-v1.h"

#include "common/treelandlogging.h"
#include "treelanduserconfig.hpp"

#include <wayland-server-core.h>

#include <QColor>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// DConfig windowColorScheme (0=light, 1=dark) → protocol color_scheme (0=light, 1=dark)
/// Values are aligned, so this only validates and forwards.
uint32_t dconfigToColorScheme(int32_t type)
{
    switch (type) {
    case TREELAND_APPEARANCE_V1_COLOR_SCHEME_LIGHT:
    case TREELAND_APPEARANCE_V1_COLOR_SCHEME_DARK:
        return static_cast<uint32_t>(type);
    default:
        qCWarning(lcTlConfig) << "Unknown dconfig windowColorScheme:" << type
                              << ", fallback to light.";
        return TREELAND_APPEARANCE_V1_COLOR_SCHEME_LIGHT;
    }
}

/// Parse "#RRGGBB" hex string to RGB components.
struct Rgb { uint32_t r, g, b; };
Rgb hexToRgb(const QString &hex)
{
    QColor c(hex);
    if (!c.isValid()) {
        qCWarning(lcTlConfig) << "Invalid color string:" << hex;
        return {0, 0, 0};
    }
    return {static_cast<uint32_t>(c.red()),
            static_cast<uint32_t>(c.green()),
            static_cast<uint32_t>(c.blue())};
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
    // If async config initialization hasn't finished (or failed), the getters
    // return DConfig default values, so push those instead of skipping.
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

AppearanceInterfaceV1::AppearanceInterfaceV1(TreelandUserConfig *config, QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , m_config(config)
    , d(std::make_unique<Private>())
{
    d->server = std::make_unique<AppearanceInterfaceV1Private>(this);

    setupConfigConnections();
    if (m_config) {
        connect(m_config, &TreelandUserConfig::configInitializeSucceed,
                this, &AppearanceInterfaceV1::onConfigChanged);
        connect(m_config, &TreelandUserConfig::configInitializeFailed,
                this, &AppearanceInterfaceV1::onConfigChanged);
    }
}

AppearanceInterfaceV1::~AppearanceInterfaceV1()
{
    for (const auto &conn : std::as_const(d->configConnections))
        QObject::disconnect(conn);
}

void AppearanceInterfaceV1::onConfigChanged()
{
    setupConfigConnections();
    for (const auto &resource : d->server->resourceMap())
        sendAll(resource->handle);
}

void AppearanceInterfaceV1::setupConfigConnections()
{
    // Rebuild subscriptions to this instance's fixed user configuration.
    for (const auto &conn : std::as_const(d->configConnections))
        QObject::disconnect(conn);
    d->configConnections.clear();

    auto *config = m_config;
    if (!config)
        return;

    // Settings change signals. Initialization completion is not watched:
    // bind_resource() and onConfigChanged() push this user's values, and
    // uninitialized getters return DConfig defaults.
    d->configConnections.append(
        connect(config, &TreelandUserConfig::cursorThemeNameChanged, this, [this] {
            const auto name = m_config->cursorThemeName();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_cursor_theme(resource->handle, name);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::cursorSizeChanged, this, [this] {
            const auto size = m_config->cursorSize();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_cursor_size(resource->handle, size, size);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::fontChanged, this, [this] {
            const auto font = m_config->font();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_font(resource->handle, font);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::monoFontChanged, this, [this] {
            const auto font = m_config->monoFont();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_monospace_font(resource->handle, font);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::fontSizeChanged, this, [this] {
            const auto size = static_cast<uint32_t>(m_config->fontSize());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_font_size(resource->handle, size);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::iconThemeNameChanged, this, [this] {
            const auto theme = m_config->iconThemeName();
            for (const auto &resource : d->server->resourceMap())
                d->server->send_icon_theme(resource->handle, theme);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::activeColorChanged, this, [this] {
            const auto rgb = hexToRgb(m_config->activeColor());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_accent_color(resource->handle, rgb.r, rgb.g, rgb.b);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowOpacityChanged, this, [this] {
            const auto opacity = wl_fixed_from_double(
                m_config->windowOpacity() / 100.0);
            for (const auto &resource : d->server->resourceMap())
                d->server->send_window_opacity(resource->handle, opacity);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowColorSchemeChanged, this, [this] {
            const auto scheme = dconfigToColorScheme(
                m_config->windowColorScheme());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_color_scheme(resource->handle, scheme);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowTitlebarHeightChanged, this, [this] {
            const auto height = static_cast<uint32_t>(
                m_config->windowTitlebarHeight());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_titlebar_height(resource->handle, height);
        }));
    d->configConnections.append(
        connect(config, &TreelandUserConfig::windowRadiusChanged, this, [this] {
            const auto radius = static_cast<uint32_t>(
                m_config->windowRadius());
            for (const auto &resource : d->server->resourceMap())
                d->server->send_corner_radius(resource->handle, radius);
        }));
}

void AppearanceInterfaceV1::sendAll(wl_resource *resource)
{
    auto *config = m_config;
    if (!config)
        return;

    d->server->send_cursor_theme(resource, config->cursorThemeName());
    const int cursorSize = config->cursorSize();
    d->server->send_cursor_size(resource, cursorSize, cursorSize);
    d->server->send_font(resource, config->font());
    d->server->send_monospace_font(resource, config->monoFont());
    d->server->send_font_size(resource, static_cast<uint32_t>(config->fontSize()));
    d->server->send_icon_theme(resource, config->iconThemeName());

    const auto rgb = hexToRgb(config->activeColor());
    d->server->send_accent_color(resource, rgb.r, rgb.g, rgb.b);

    d->server->send_window_opacity(resource,
        wl_fixed_from_double(config->windowOpacity() / 100.0));

    d->server->send_color_scheme(resource, dconfigToColorScheme(config->windowColorScheme()));
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
