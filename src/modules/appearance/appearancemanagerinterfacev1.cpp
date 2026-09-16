// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearancemanagerinterfacev1.h"

#include "qwayland-server-treeland-appearance-manager-unstable-v1.h"

#include "common/treelandlogging.h"
#include "seat/helper.h"
#include "treelanduserconfig.hpp"

#include <wayland-server-core.h>

#include <QColor>
#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Protocol color_scheme (0=light, 1=dark) → DConfig windowColorScheme (0=light, 1=dark)
/// Values are aligned, so this only validates and forwards.
int32_t colorSchemeToDConfig(uint32_t scheme)
{
    switch (scheme) {
    case TREELAND_APPEARANCE_MANAGER_V1_COLOR_SCHEME_LIGHT:
    case TREELAND_APPEARANCE_MANAGER_V1_COLOR_SCHEME_DARK:
        return static_cast<int32_t>(scheme);
    default:
        qCWarning(lcTlConfig) << "Unknown color_scheme:" << scheme
                              << ", ignoring.";
        return -1; // caller should check
    }
}

/// Convert RGB components to "#RRGGBB" hex string for DConfig.
QString rgbToHex(uint32_t r, uint32_t g, uint32_t b)
{
    QColor c(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
    return c.name(QColor::HexRgb); // #RRGGBB
}

/// Check whether the user config has completed asynchronous initialization.
bool isUserConfigInitialized(TreelandUserConfig *config)
{
    if (!config)
        return false;
    return config->isInitializeSucceeded();
}

/// Guard against null Helper, null config, or incomplete async config init.
///
/// FIXME(rewine): Treeland is a multi-user compositor. Helper::config() exposes the
/// CURRENT user's config instance; this manager currently has no per-user
/// resource tracking, so a request from one user's client could mutate another
/// user's config once per-user client credentials are available. Binding must be
/// restricted to the privileged control center of the active user, and set_*
/// requests must be validated against the requester's user identity.
///
/// During the async init window (before configInitializeSucceed), setters are
/// dropped silently to avoid writing before DConfig is ready; log once per drop
/// so the failure is observable.
#define ENSURE_CONFIG()                                            \
    do {                                                           \
        if (!Helper::instance() || !Helper::instance()->config()) \
            return;                                                \
        if (!isUserConfigInitialized(Helper::instance()->config())) { \
            qCWarning(lcTlConfig) << "Appearance set_* request dropped:" \
                                  << "user config not initialized yet"; \
            return;                                                \
        }                                                          \
    } while (false)

} // namespace

// ---------------------------------------------------------------------------
// AppearanceManagerInterfaceV1Private
// ---------------------------------------------------------------------------

class AppearanceManagerInterfaceV1Private
    : public QtWaylandServer::treeland_appearance_manager_v1
{
public:
    explicit AppearanceManagerInterfaceV1Private(AppearanceManagerInterfaceV1 *_q);
    ~AppearanceManagerInterfaceV1Private() override = default;

    wl_global *global() const { return m_global; }

    AppearanceManagerInterfaceV1 *q = nullptr;

protected:
    void destroy(Resource *resource) override;
    void set_cursor_theme(Resource *resource, const QString &name) override;
    void set_cursor_size(Resource *resource, uint32_t width, uint32_t height) override;
    void set_font(Resource *resource, const QString &font_name) override;
    void set_monospace_font(Resource *resource, const QString &font_name) override;
    void set_font_size(Resource *resource, uint32_t size) override;
    void set_icon_theme(Resource *resource, const QString &theme_name) override;
    void set_accent_color(Resource *resource, uint32_t r, uint32_t g, uint32_t b) override;
    void set_window_opacity(Resource *resource, wl_fixed_t opacity) override;
    void set_color_scheme(Resource *resource, uint32_t scheme) override;
    void set_titlebar_height(Resource *resource, uint32_t height) override;
    void set_corner_radius(Resource *resource, uint32_t radius) override;
};

AppearanceManagerInterfaceV1Private::AppearanceManagerInterfaceV1Private(AppearanceManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_appearance_manager_v1()
    , q(_q)
{
}

void AppearanceManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void AppearanceManagerInterfaceV1Private::set_cursor_theme([[maybe_unused]] Resource *resource,
                                                            const QString &name)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setCursorThemeName(name);
}

void AppearanceManagerInterfaceV1Private::set_cursor_size([[maybe_unused]] Resource *resource,
                                                           uint32_t width, uint32_t height)
{
    ENSURE_CONFIG();
    if (width != height)
        qCWarning(lcTlConfig) << "Non-square cursor size requested:" << width << "x" << height
                              << ", height is ignored.";
    Helper::instance()->config()->setCursorSize(static_cast<int>(width));
}

void AppearanceManagerInterfaceV1Private::set_font([[maybe_unused]] Resource *resource,
                                                    const QString &font_name)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setFont(font_name);
}

void AppearanceManagerInterfaceV1Private::set_monospace_font([[maybe_unused]] Resource *resource,
                                                              const QString &font_name)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setMonoFont(font_name);
}

void AppearanceManagerInterfaceV1Private::set_font_size([[maybe_unused]] Resource *resource,
                                                         uint32_t size)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setFontSize(static_cast<int>(size));
}

void AppearanceManagerInterfaceV1Private::set_icon_theme([[maybe_unused]] Resource *resource,
                                                          const QString &theme_name)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setIconThemeName(theme_name);
}

void AppearanceManagerInterfaceV1Private::set_accent_color([[maybe_unused]] Resource *resource,
                                                            uint32_t r, uint32_t g, uint32_t b)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setActiveColor(rgbToHex(r, g, b));
}

void AppearanceManagerInterfaceV1Private::set_window_opacity([[maybe_unused]] Resource *resource,
                                                              wl_fixed_t opacity)
{
    ENSURE_CONFIG();
    const double value = wl_fixed_to_double(opacity);
    if (value < 0.0 || value > 1.0) {
        qCWarning(lcTlConfig) << "window_opacity out of range [0.0, 1.0]:" << value;
        return;
    }
    Helper::instance()->config()->setWindowOpacity(static_cast<int>(std::lround(value * 100.0)));
}

void AppearanceManagerInterfaceV1Private::set_color_scheme([[maybe_unused]] Resource *resource,
                                                            uint32_t scheme)
{
    ENSURE_CONFIG();
    const auto dconfigType = colorSchemeToDConfig(scheme);
    if (dconfigType < 0)
        return; // invalid scheme, silently ignored per protocol spec
    Helper::instance()->config()->setWindowColorScheme(dconfigType);
    Helper::syncPaletteTypeWithWindowColorScheme(dconfigType);
}

void AppearanceManagerInterfaceV1Private::set_titlebar_height([[maybe_unused]] Resource *resource,
                                                               uint32_t height)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setWindowTitlebarHeight(static_cast<int>(height));
}

void AppearanceManagerInterfaceV1Private::set_corner_radius([[maybe_unused]] Resource *resource,
                                                             uint32_t radius)
{
    ENSURE_CONFIG();
    Helper::instance()->config()->setWindowRadius(static_cast<int>(radius));
}

// ---------------------------------------------------------------------------
// AppearanceManagerInterfaceV1
// ---------------------------------------------------------------------------

struct AppearanceManagerInterfaceV1::Private
{
    std::unique_ptr<AppearanceManagerInterfaceV1Private> server;
};

AppearanceManagerInterfaceV1::AppearanceManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<Private>())
{
    d->server = std::make_unique<AppearanceManagerInterfaceV1Private>(this);
}

AppearanceManagerInterfaceV1::~AppearanceManagerInterfaceV1() = default;

QByteArrayView AppearanceManagerInterfaceV1::interfaceName() const
{
    return d->server->interfaceName();
}

void AppearanceManagerInterfaceV1::create(WServer *server)
{
    d->server->init(server->handle(), InterfaceVersion);
}

void AppearanceManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->server->globalRemove();
}

wl_global *AppearanceManagerInterfaceV1::global() const
{
    return d->server->global();
}
