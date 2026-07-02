// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmanagerinterfacev1.h"
#include "surfacewrapper.h"

#include "qwayland-server-treeland-personalization-manager-v1.h"

#include "seat/helper.h"
#include "common/treelandlogging.h"
#include "treelanduserconfig.hpp"

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_layer_shell_v1.h>

#include <wlayersurface.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwlayershellv1.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <optional>
#include <sys/socket.h>
#include <unistd.h>

DCORE_USE_NAMESPACE

static QList<PersonalizationWindowContextV1 *> s_windowContexts;
static QList<PersonalizationCursorContextV1 *> s_cursorContexts;
static QList<PersonalizationFontContextV1 *> s_fontContexts;
static QList<PersonalizationAppearanceContextV1 *> s_appearanceContexts;

static std::optional<int32_t> protocolWindowThemeTypeToDConfig(uint32_t type)
{
    switch (type) {
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT:
        return 1;
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK:
        return 2;
    case TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_AUTO:
        qCCritical(lcTlConfig)
            << "Protocol window theme type AUTO is not supported by dconfig.";
        return std::nullopt;
    default:
        qCWarning(lcTlConfig) << "Unknown protocol window theme type:" << type;
        return std::nullopt;
    }
}

static uint32_t dconfigWindowThemeTypeToProtocol(int32_t type)
{
    switch (type) {
    case 1:
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT;
    case 2:
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_DARK;
    default:
        qCWarning(lcTlConfig)
            << "Unknown dconfig windowThemeType:" << type << ", fallback to light.";
        return TREELAND_PERSONALIZATION_APPEARANCE_CONTEXT_V1_THEME_TYPE_LIGHT;
    }
}

class PersonalizationManagerInterfaceV1Private
    : public QtWaylandServer::treeland_personalization_manager_v1
{
public:
    PersonalizationManagerInterfaceV1Private(PersonalizationManagerInterfaceV1 *_q);
    ~PersonalizationManagerInterfaceV1Private() override = default;

    wl_global *global() const;

    uid_t userId = 0;
    QString cacheDirectory;
    QString settingFile;
    QString iniMetaData;
    PersonalizationManagerInterfaceV1 *q;

protected:
    void get_window_context(Resource *resource, uint32_t id, struct ::wl_resource *surface) override;
    void get_cursor_context(Resource *resource, uint32_t id) override;
    void get_font_context(Resource *resource, uint32_t id) override;
    void get_appearance_context(Resource *resource, uint32_t id) override;
};

PersonalizationManagerInterfaceV1Private::PersonalizationManagerInterfaceV1Private(PersonalizationManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_personalization_manager_v1()
    , q(_q)
{
}

wl_global *PersonalizationManagerInterfaceV1Private::global() const
{
    return m_global;
}

void PersonalizationManagerInterfaceV1Private::get_window_context(Resource *resource,
                                                                  uint32_t id,
                                                                  struct ::wl_resource *surface)
{
    if (!surface) {
        wl_resource_post_error(resource->handle, 0, "surface resource is NULL!");
        return;
    }
    auto *wlrSurface = wlr_surface_from_resource(surface);

    wl_resource *windowContextResource = wl_resource_create(resource->client(),
                                                            &treeland_personalization_window_context_v1_interface,
                                                            resource->version(),
                                                            id);
    if (!windowContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new PersonalizationWindowContextV1(windowContextResource,
                                                       wlrSurface);
    s_windowContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [context]() {
        s_windowContexts.removeOne(context);
    });

    Q_EMIT q->windowContextCreated(context);
}

void PersonalizationManagerInterfaceV1Private::get_cursor_context(Resource *resource, uint32_t id)
{
    wl_resource *cursorContextResource = wl_resource_create(resource->client(),
                                                               &treeland_personalization_cursor_context_v1_interface,
                                                               resource->version(),
                                                               id);
    if (!cursorContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new PersonalizationCursorContextV1(cursorContextResource);
    s_cursorContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [context]() {
        s_cursorContexts.removeOne(context);
    });

    Q_EMIT q->onCursorContextCreated(context);
}

void PersonalizationManagerInterfaceV1Private::get_font_context(Resource *resource, uint32_t id)
{
    wl_resource *fontContextResource = wl_resource_create(resource->client(),
                                                            &treeland_personalization_font_context_v1_interface,
                                                            resource->version(),
                                                            id);
    if (!fontContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new PersonalizationFontContextV1(fontContextResource);

    s_fontContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [context]() {
        s_fontContexts.removeOne(context);
    });

    Q_EMIT q->onFontContextCreated(context);
}

void PersonalizationManagerInterfaceV1Private::get_appearance_context(Resource *resource, uint32_t id)
{
    wl_resource *appearanceContextResource = wl_resource_create(resource->client(),
                                                          &treeland_personalization_appearance_context_v1_interface,
                                                          resource->version(),
                                                          id);
    if (!appearanceContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new PersonalizationAppearanceContextV1(appearanceContextResource);

    s_appearanceContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [context]() {
        s_appearanceContexts.removeOne(context);
    });

    Q_EMIT q->onAppearanceContextCreated(context);
}

static bool isXdgToplevelSurface(wlr_surface *surface)
{
    if (!surface)
        return false;

    auto *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
    return xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL;
}

class PersonalizationWindowContextV1Private
    : public QtWaylandServer::treeland_personalization_window_context_v1
{
public:
    PersonalizationWindowContextV1Private(PersonalizationWindowContextV1 *_q,
                                          wl_resource *resource,
                                          wlr_surface *surface);
    ~PersonalizationWindowContextV1Private() override = default;

    PersonalizationWindowContextV1 *q = nullptr;
    wlr_surface *surface = nullptr;

    int32_t backgroundType = 0;
    int32_t cornerRadius = 0;
    Shadow shadow;
    Border border;
    PersonalizationWindowContextV1::WindowStates states;
    PersonalizationWindowContextV1::DecorationState shadowState = PersonalizationWindowContextV1::DecorationState::NotSet;
    PersonalizationWindowContextV1::DecorationState borderState = PersonalizationWindowContextV1::DecorationState::NotSet;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_blend_mode(Resource *resource, int32_t type) override;
    void set_round_corner_radius(Resource *resource, int32_t radius) override;
    void set_shadow(Resource *resource, int32_t radius, int32_t offset_x, int32_t offset_y, int32_t r, int32_t g, int32_t b, int32_t a) override;
    void set_border(Resource *resource, int32_t width, int32_t r, int32_t g, int32_t b, int32_t a) override;
    void set_titlebar(Resource *resource, int32_t mode) override;
    void set_shadow_enabled(Resource *resource, uint32_t enabled) override;
    void set_border_enabled(Resource *resource, uint32_t enabled) override;
};

PersonalizationWindowContextV1Private::PersonalizationWindowContextV1Private(
    PersonalizationWindowContextV1 *_q,
    wl_resource *resource,
    wlr_surface *_surface)
    : QtWaylandServer::treeland_personalization_window_context_v1(resource)
    , q(_q)
    , surface(_surface)
{
    if (isXdgToplevelSurface(_surface)) {
        shadowState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
        shadow = Shadow{ 40, QPoint{ 0, 10 }, QColor{ 0, 0, 0, 102 } };
        borderState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
        border = Border{ 1, QColor{ 255, 255, 255, 26 } };
    }
}

void PersonalizationWindowContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void PersonalizationWindowContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationWindowContextV1Private::set_blend_mode([[maybe_unused]] Resource *resource, int32_t type)
{
    backgroundType = type;
    Q_EMIT q->backgroundTypeChanged();
}

void PersonalizationWindowContextV1Private::set_round_corner_radius([[maybe_unused]] Resource *resource, int32_t radius)
{
    cornerRadius = radius;
    Q_EMIT q->cornerRadiusChanged();
}

void PersonalizationWindowContextV1Private::set_shadow([[maybe_unused]] Resource *resource,
                                                        int32_t radius,
                                                        int32_t offset_x,
                                                        int32_t offset_y,
                                                        int32_t r,
                                                        int32_t g,
                                                        int32_t b,
                                                        int32_t a)
{
    shadow = Shadow{ radius, QPoint{ offset_x, offset_y }, QColor{ r, g, b, a } };
    shadowState = PersonalizationWindowContextV1::DecorationState::Custom;
    Q_EMIT q->shadowChanged();
}

void PersonalizationWindowContextV1Private::set_border([[maybe_unused]] Resource *resource,
                                                        int32_t width,
                                                        int32_t r,
                                                        int32_t g,
                                                        int32_t b,
                                                        int32_t a)
{
    border = Border{ width, QColor{ r, g, b, a } };
    borderState = PersonalizationWindowContextV1::DecorationState::Custom;
    Q_EMIT q->borderChanged();
}

void PersonalizationWindowContextV1Private::set_titlebar([[maybe_unused]] Resource *resource, int32_t mode)
{
    states.setFlag(
        PersonalizationWindowContextV1::WindowState::NoTitleBar,
        mode == TREELAND_PERSONALIZATION_WINDOW_CONTEXT_V1_ENABLE_MODE_DISABLE);
    Q_EMIT q->windowStateChanged();
}

void PersonalizationWindowContextV1Private::set_shadow_enabled([[maybe_unused]] Resource *resource, uint32_t enabled)
{
    if (enabled) {
        if (shadowState == PersonalizationWindowContextV1::DecorationState::NotSet) {
            shadow = Shadow{ 40, QPoint{ 0, 10 }, QColor{ 0, 0, 0, 102 } };
            shadowState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
            Q_EMIT q->shadowChanged();
        }
        auto oldState = shadowState;
        shadowState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
        if (oldState != shadowState) {
            Q_EMIT q->shadowStateChanged();
        }
    } else {
        shadow = Shadow{};
        shadowState = PersonalizationWindowContextV1::DecorationState::NotSet;
        Q_EMIT q->shadowChanged();
        Q_EMIT q->shadowStateChanged();
    }
}

void PersonalizationWindowContextV1Private::set_border_enabled([[maybe_unused]] Resource *resource, uint32_t enabled)
{
    if (enabled) {
        if (borderState == PersonalizationWindowContextV1::DecorationState::NotSet) {
            border = Border{ 1, QColor{ 255, 255, 255, 26 } };
            borderState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
            Q_EMIT q->borderChanged();
        }
        auto oldState = borderState;
        borderState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
        if (oldState != borderState) {
            Q_EMIT q->borderStateChanged();
        }
    } else {
        border = Border{};
        borderState = PersonalizationWindowContextV1::DecorationState::NotSet;
        Q_EMIT q->borderChanged();
        Q_EMIT q->borderStateChanged();
    }
}

PersonalizationWindowContextV1::PersonalizationWindowContextV1(
    wl_resource *resource,
    wlr_surface *surface)
    : QObject(nullptr)
    , d(new PersonalizationWindowContextV1Private(this, resource, surface))
{
}

PersonalizationWindowContextV1::~PersonalizationWindowContextV1() = default;

wl_resource *PersonalizationWindowContextV1::resource() const
{
    return d->resource()->handle;
}

wlr_surface *PersonalizationWindowContextV1::surface() const
{
    return d->surface;
}

int32_t PersonalizationWindowContextV1::backgroundType() const
{
    return d->backgroundType;
}

int32_t PersonalizationWindowContextV1::cornerRadius() const
{
    return d->cornerRadius;
}

Shadow PersonalizationWindowContextV1::shadow() const
{
    return d->shadow;
}

Border PersonalizationWindowContextV1::border() const
{
    return d->border;
}

PersonalizationWindowContextV1::DecorationState PersonalizationWindowContextV1::shadowState() const
{
    return d->shadowState;
}

PersonalizationWindowContextV1::DecorationState PersonalizationWindowContextV1::borderState() const
{
    return d->borderState;
}

PersonalizationWindowContextV1 *PersonalizationWindowContextV1::get(wl_resource *resource)
{
    for (auto *context : std::as_const(s_windowContexts)) {
        if (context->resource() == resource) {
            return context;
        }
    }

    return nullptr;
}

PersonalizationWindowContextV1 *PersonalizationWindowContextV1::getWindowContext(WSurface *surface)
{
    for (auto *context : std::as_const(s_windowContexts)) {
        if (context->surface() == surface->handle()->handle()) {
            return context;
        }
    }

    return nullptr;
}

PersonalizationWindowContextV1::WindowStates PersonalizationWindowContextV1::states() const
{
    return d->states;
}

class PersonalizationCursorContextV1Private
    : public QtWaylandServer::treeland_personalization_cursor_context_v1
{
public:
    PersonalizationCursorContextV1Private(PersonalizationCursorContextV1 *_q,
                                          wl_resource *resource);
    ~PersonalizationCursorContextV1Private() override = default;

    PersonalizationCursorContextV1 *q = nullptr;

    QSize size;
    QString theme;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_theme(Resource *resource, const QString &name) override;
    void get_theme(Resource *resource) override;
    void set_size(Resource *resource, uint32_t size) override;
    void get_size(Resource *resource) override;
    void commit(Resource *resource) override;
};

PersonalizationCursorContextV1Private::PersonalizationCursorContextV1Private(
    PersonalizationCursorContextV1 *_q, wl_resource *resource)
    : QtWaylandServer::treeland_personalization_cursor_context_v1(resource)
    , q(_q)
{
}

void PersonalizationCursorContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void PersonalizationCursorContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationCursorContextV1Private::set_theme([[maybe_unused]] Resource *resource, const QString &name)
{
    theme = name;
}

void PersonalizationCursorContextV1Private::get_theme([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->getTheme();
}

void PersonalizationCursorContextV1Private::set_size([[maybe_unused]] Resource *resource, uint32_t _size)
{
    size = QSize(_size, _size);
}

void PersonalizationCursorContextV1Private::get_size([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->getSize();
}

void PersonalizationCursorContextV1Private::commit([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->commit(q);
}

PersonalizationCursorContextV1::PersonalizationCursorContextV1(wl_resource *resource)
    : QObject(nullptr)
    , d(new PersonalizationCursorContextV1Private(this, resource))
{
}

PersonalizationCursorContextV1::~PersonalizationCursorContextV1() = default;

wl_resource *PersonalizationCursorContextV1::resource() const
{
    return d->resource()->handle;
}

QSize PersonalizationCursorContextV1::size() const
{
    return d->size;
}

QString PersonalizationCursorContextV1::theme() const
{
    return d->theme;
}

void PersonalizationCursorContextV1::setTheme(const QString &theme)
{
    if (d->theme == theme) {
        return;
    }

    d->theme = theme;
}

void PersonalizationCursorContextV1::setSize(const QSize &size)
{
    if (d->size == size) {
        return;
    }

    d->size = size;
}

void PersonalizationCursorContextV1::verify(bool verified)
{
    d->send_verfity(verified);
}

void PersonalizationCursorContextV1::sendTheme()
{
    d->send_theme(d->theme);
}

void PersonalizationCursorContextV1::sendSize()
{
    d->send_size(d->size.width());
}

PersonalizationCursorContextV1 *PersonalizationCursorContextV1::get(wl_resource *resource)
{
    for (auto *context : std::as_const(s_cursorContexts)) {
        if (context->resource() == resource) {
            return context;
        }
    }

    return nullptr;
}

class PersonalizationAppearanceContextV1Private
    : public QtWaylandServer::treeland_personalization_appearance_context_v1
{
public:
    PersonalizationAppearanceContextV1Private(PersonalizationAppearanceContextV1 *_q,
                                              wl_resource *resource);
    ~PersonalizationAppearanceContextV1Private() override = default;

    PersonalizationAppearanceContextV1 *q = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_round_corner_radius(Resource *resource, int32_t radius) override;
    void get_round_corner_radius(Resource *resource) override;
    void set_icon_theme(Resource *resource, const QString &theme_name) override;
    void get_icon_theme(Resource *resource) override;
    void set_active_color(Resource *resource, const QString &color) override;
    void get_active_color(Resource *resource) override;
    void set_window_opacity(Resource *resource, uint32_t opacity) override;
    void get_window_opacity(Resource *resource) override;
    void set_window_theme_type(Resource *resource, uint32_t type) override;
    void get_window_theme_type(Resource *resource) override;
    void set_window_titlebar_height(Resource *resource, uint32_t height) override;
    void get_window_titlebar_height(Resource *resource) override;
};

PersonalizationAppearanceContextV1Private::PersonalizationAppearanceContextV1Private(
    PersonalizationAppearanceContextV1 *_q,
    wl_resource *resource)
    : QtWaylandServer::treeland_personalization_appearance_context_v1(resource)
    , q(_q)
{
}

void PersonalizationAppearanceContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void PersonalizationAppearanceContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationAppearanceContextV1Private::set_round_corner_radius([[maybe_unused]] Resource *resource, int32_t radius)
{
    Q_EMIT q->roundCornerRadiusChanged(radius);
}

void PersonalizationAppearanceContextV1Private::get_round_corner_radius([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestRoundCornerRadius();
}

void PersonalizationAppearanceContextV1Private::set_icon_theme([[maybe_unused]] Resource *resource, const QString &theme_name)
{
    Q_EMIT q->iconThemeChanged(theme_name);
}

void PersonalizationAppearanceContextV1Private::get_icon_theme([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestIconTheme();
}

void PersonalizationAppearanceContextV1Private::set_active_color([[maybe_unused]] Resource *resource, const QString &color)
{
    Q_EMIT q->activeColorChanged(color);
}

void PersonalizationAppearanceContextV1Private::get_active_color([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestActiveColor();
}

void PersonalizationAppearanceContextV1Private::set_window_opacity([[maybe_unused]] Resource *resource, uint32_t opacity)
{
    Q_EMIT q->windowOpacityChanged(opacity);
}

void PersonalizationAppearanceContextV1Private::get_window_opacity([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestWindowOpacity();
}

void PersonalizationAppearanceContextV1Private::set_window_theme_type([[maybe_unused]] Resource *resource, uint32_t type)
{
    Q_EMIT q->windowThemeTypeChanged(type);
}

void PersonalizationAppearanceContextV1Private::get_window_theme_type([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestWindowThemeType();
}

void PersonalizationAppearanceContextV1Private::set_window_titlebar_height([[maybe_unused]] Resource *resource, uint32_t height)
{
    Q_EMIT q->titlebarHeightChanged(height);
}

void PersonalizationAppearanceContextV1Private::get_window_titlebar_height([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestWindowTitlebarHeight();
}

PersonalizationAppearanceContextV1::PersonalizationAppearanceContextV1(wl_resource *resource)
    : QObject(nullptr)
    , d(new PersonalizationAppearanceContextV1Private(this, resource))
{
}

PersonalizationAppearanceContextV1::~PersonalizationAppearanceContextV1() = default;

wl_resource *PersonalizationAppearanceContextV1::resource() const
{
    return d->resource()->handle;
}

void PersonalizationAppearanceContextV1::setRoundCornerRadius(int32_t radius)
{
    Q_EMIT roundCornerRadiusChanged(radius);
}

void PersonalizationAppearanceContextV1::sendRoundCornerRadius(int32_t radius)
{
    d->send_round_corner_radius(radius);
}

void PersonalizationAppearanceContextV1::setIconTheme(const QString &theme)
{
    Q_EMIT iconThemeChanged(theme);
}

void PersonalizationAppearanceContextV1::sendIconTheme(const QString &theme)
{
    d->send_icon_theme(theme);
}

void PersonalizationAppearanceContextV1::setActiveColor(const QString &color)
{
    Q_EMIT activeColorChanged(color);
}

void PersonalizationAppearanceContextV1::sendActiveColor(const QString &color)
{
    d->send_active_color(color);
}

void PersonalizationAppearanceContextV1::setWindowOpacity(uint32_t opacity)
{
    Q_EMIT windowOpacityChanged(opacity);
}

void PersonalizationAppearanceContextV1::sendWindowOpacity(uint32_t opacity)
{
    d->send_window_opacity(opacity);
}

void PersonalizationAppearanceContextV1::setWindowThemeType(uint32_t type)
{
    Q_EMIT windowThemeTypeChanged(type);
}

void PersonalizationAppearanceContextV1::sendWindowThemeType(uint32_t type)
{
    d->send_window_theme_type(type);
}

void PersonalizationAppearanceContextV1::setWindowTitlebarHeight(uint32_t height)
{
    Q_EMIT titlebarHeightChanged(height);
}

void PersonalizationAppearanceContextV1::sendWindowTitlebarHeight(uint32_t height)
{
    d->send_window_titlebar_height(height);
}

PersonalizationAppearanceContextV1 *PersonalizationAppearanceContextV1::get(wl_resource *resource)
{
    for (auto *context : std::as_const(s_appearanceContexts)) {
        if (context->resource() == resource) {
            return context;
        }
    }

    return nullptr;
}

class PersonalizationFontContextV1Private
    : public QtWaylandServer::treeland_personalization_font_context_v1
{
public:
    PersonalizationFontContextV1Private(PersonalizationFontContextV1 *_q, wl_resource *resource);
    ~PersonalizationFontContextV1Private() override = default;

    PersonalizationFontContextV1 *q = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_font(Resource *resource, const QString &font) override;
    void get_font(Resource *resource) override;
    void set_monospace_font(Resource *resource, const QString &font) override;
    void get_monospace_font(Resource *resource) override;
    void set_font_size(Resource *resource, uint32_t size) override;
    void get_font_size(Resource *resource) override;
};

PersonalizationFontContextV1Private::PersonalizationFontContextV1Private(
    PersonalizationFontContextV1 *_q,
    wl_resource *resource)
    : QtWaylandServer::treeland_personalization_font_context_v1(resource)
    , q(_q)
{
}

void PersonalizationFontContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void PersonalizationFontContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PersonalizationFontContextV1Private::set_font([[maybe_unused]] Resource *resource, const QString &font)
{
    Q_EMIT q->fontChanged(font);
}

void PersonalizationFontContextV1Private::get_font([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestFont();
}

void PersonalizationFontContextV1Private::set_monospace_font([[maybe_unused]] Resource *resource, const QString &font)
{
    Q_EMIT q->monoFontChanged(font);
}

void PersonalizationFontContextV1Private::get_monospace_font([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestMonoFont();
}

void PersonalizationFontContextV1Private::set_font_size([[maybe_unused]] Resource *resource, uint32_t size)
{
    Q_EMIT q->fontSizeChanged(size);
}

void PersonalizationFontContextV1Private::get_font_size([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestFontSize();
}

PersonalizationFontContextV1::PersonalizationFontContextV1(wl_resource *resource)
    : QObject(nullptr)
    , d(new PersonalizationFontContextV1Private(this, resource))
{
}

PersonalizationFontContextV1::~PersonalizationFontContextV1() = default;

wl_resource *PersonalizationFontContextV1::resource() const
{
    return d->resource()->handle;
}

void PersonalizationFontContextV1::sendFont(const QString &font)
{
    d->send_font(font);
}

void PersonalizationFontContextV1::sendMonospaceFont(const QString &font)
{
    d->send_monospace_font(font);
}

void PersonalizationFontContextV1::sendFontSize(uint32_t size)
{
    d->send_font_size(size);
}

PersonalizationFontContextV1 *PersonalizationFontContextV1::get(wl_resource *resource)
{
    for (auto *context : std::as_const(s_fontContexts)) {
        if (context->resource() == resource) {
            return context;
        }
    }

    return nullptr;
}

PersonalizationManagerInterfaceV1::PersonalizationManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new PersonalizationManagerInterfaceV1Private(this))
{
    Q_INIT_RESOURCE(default_background);

    if (qgetenv("TREELAND_RUN_MODE") == "user") {
        setUserId(getgid());
    }
}

PersonalizationManagerInterfaceV1::~PersonalizationManagerInterfaceV1()
{
    Q_CLEANUP_RESOURCE(default_background);
}

void PersonalizationManagerInterfaceV1::onCursorContextCreated(PersonalizationCursorContextV1 *context)
{
    connect(context,
            &PersonalizationCursorContextV1::commit,
            this,
            &PersonalizationManagerInterfaceV1::onCursorCommit);
    connect(context,
            &PersonalizationCursorContextV1::getTheme,
            context,
            &PersonalizationCursorContextV1::sendTheme);
    connect(context,
            &PersonalizationCursorContextV1::getSize,
            context,
            &PersonalizationCursorContextV1::sendSize);

    context->blockSignals(true);
    context->setTheme(Helper::instance()->config()->cursorThemeName());
    auto size = Helper::instance()->config()->cursorSize();
    context->setSize(QSize(size, size));
    connect(Helper::instance()->config(),
            &TreelandUserConfig::cursorThemeNameChanged,
            context,
            [context]() {
                context->setTheme(Helper::instance()->config()->cursorThemeName());
            });
    connect(Helper::instance()->config(),
            &TreelandUserConfig::cursorSizeChanged,
            context,
            [context]() {
                auto size = Helper::instance()->config()->cursorSize();
                context->setSize(QSize(size, size));
            });
    context->blockSignals(false);
}

void PersonalizationManagerInterfaceV1::onAppearanceContextCreated(PersonalizationAppearanceContextV1 *context)
{
    connect(context, &PersonalizationAppearanceContextV1::roundCornerRadiusChanged, this, [](int32_t radius) {
        Helper::instance()->config()->setWindowRadius(radius);
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendRoundCornerRadius(radius);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::iconThemeChanged, this, [](const QString &theme) {
        Helper::instance()->config()->setIconThemeName(theme);
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendIconTheme(theme);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::activeColorChanged, this, [](const QString &color) {
        Helper::instance()->config()->setActiveColor(color);
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendActiveColor(color);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::windowOpacityChanged, this, [](uint32_t opacity) {
        Helper::instance()->config()->setWindowOpacity(opacity);
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendWindowOpacity(opacity);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::windowThemeTypeChanged, this, [](uint32_t type) {
        const auto dconfigType = protocolWindowThemeTypeToDConfig(type);
        if (dconfigType.has_value()) {
            Helper::instance()->config()->setWindowThemeType(*dconfigType);
            Helper::syncPaletteTypeWithWindowThemeType(*dconfigType);
        }
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendWindowThemeType(type);
        }
    });
    connect(context, &PersonalizationAppearanceContextV1::titlebarHeightChanged, this, [](uint32_t height) {
        Helper::instance()->config()->setWindowTitlebarHeight(height);
        for (auto *c : std::as_const(s_appearanceContexts)) {
            c->sendWindowTitlebarHeight(height);
        }
    });

    connect(context, &PersonalizationAppearanceContextV1::requestRoundCornerRadius, context, [this, context] {
        context->setRoundCornerRadius(windowRadius());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestIconTheme, context, [this, context] {
        context->setIconTheme(iconTheme());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestActiveColor, context, [context] {
        context->setActiveColor(Helper::instance()->config()->activeColor());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowOpacity, context, [context] {
        context->setWindowOpacity(Helper::instance()->config()->windowOpacity());
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowThemeType, context, [context] {
        const auto protocolType = dconfigWindowThemeTypeToProtocol(
            Helper::instance()->config()->windowThemeType());
        context->setWindowThemeType(protocolType);
    });

    connect(context, &PersonalizationAppearanceContextV1::requestWindowTitlebarHeight, context, [context] {
        context->setWindowTitlebarHeight(Helper::instance()->config()->windowTitlebarHeight());
    });

    context->blockSignals(true);

    context->setRoundCornerRadius(Helper::instance()->config()->windowRadius());
    context->setIconTheme(Helper::instance()->config()->iconThemeName());
    context->setActiveColor(Helper::instance()->config()->activeColor());
    context->setWindowOpacity(Helper::instance()->config()->windowOpacity());
    context->setWindowThemeType(dconfigWindowThemeTypeToProtocol(
        Helper::instance()->config()->windowThemeType()));
    context->setWindowTitlebarHeight(Helper::instance()->config()->windowTitlebarHeight());

    context->blockSignals(false);
}

void PersonalizationManagerInterfaceV1::onFontContextCreated(PersonalizationFontContextV1 *context)
{
    connect(Helper::instance()->config(), &TreelandUserConfig::fontChanged, context, [context] {
        context->sendFont(Helper::instance()->config()->font());
    });
    connect(Helper::instance()->config(), &TreelandUserConfig::monoFontChanged, context, [context] {
        context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    });
    connect(Helper::instance()->config(), &TreelandUserConfig::fontSizeChanged, context, [context] {
        context->sendFontSize(Helper::instance()->config()->fontSize());
    });

    connect(context, &PersonalizationFontContextV1::requestFont, context, [context] {
        context->sendFont(Helper::instance()->config()->font());
    });
    connect(context, &PersonalizationFontContextV1::requestMonoFont, context, [context] {
        context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    });
    connect(context, &PersonalizationFontContextV1::requestFontSize, context, [context] {
        context->sendFontSize(Helper::instance()->config()->fontSize());
    });

    connect(context, &PersonalizationFontContextV1::fontChanged, Helper::instance()->config(), &TreelandUserConfig::setFont);
    connect(context, &PersonalizationFontContextV1::monoFontChanged, Helper::instance()->config(), &TreelandUserConfig::setMonoFont);
    connect(context, &PersonalizationFontContextV1::fontSizeChanged, Helper::instance()->config(), &TreelandUserConfig::setFontSize);

    context->blockSignals(true);

    context->sendFont(Helper::instance()->config()->font());
    context->sendMonospaceFont(Helper::instance()->config()->monoFont());
    context->sendFontSize(Helper::instance()->config()->fontSize());

    context->blockSignals(false);
}

void PersonalizationManagerInterfaceV1::onCursorCommit(PersonalizationCursorContextV1 *context)
{
    if (!context->size().isValid() || context->theme().isEmpty()) {
        context->verify(false);
    }

    setCursorTheme(context->theme());
    setCursorSize(context->size());

    context->verify(true);
}

uid_t PersonalizationManagerInterfaceV1::userId()
{
    return d->userId;
}

void PersonalizationManagerInterfaceV1::setUserId(uid_t uid)
{
    if (d->userId == uid) {
        return;
    }

    d->userId = uid;
    Q_EMIT userIdChanged(uid);
}

QString PersonalizationManagerInterfaceV1::cursorTheme()
{
    return Helper::instance()->config()->cursorThemeName();
}

void PersonalizationManagerInterfaceV1::setCursorTheme(const QString &name)
{
    Helper::instance()->config()->setCursorThemeName(name);

    Q_EMIT cursorThemeChanged(name);
}

QSize PersonalizationManagerInterfaceV1::cursorSize()
{
    int size = Helper::instance()->config()->cursorSize();

    return QSize(size, size);
}

void PersonalizationManagerInterfaceV1::setCursorSize(const QSize &size)
{
    Helper::instance()->config()->setCursorSize(size.width());

    Q_EMIT cursorSizeChanged(size);
}

int32_t PersonalizationManagerInterfaceV1::windowRadius() const
{
    return Helper::instance()->config()->windowRadius();
}

QString PersonalizationManagerInterfaceV1::iconTheme() const
{
    return Helper::instance()->config()->iconThemeName();
}

Personalization::Personalization(WToplevelSurface *target,
                                 PersonalizationManagerInterfaceV1 *manager,
                                 SurfaceWrapper *parent)
    : QObject(parent)
    , m_target(target)
    , m_manager(manager)
{
    if (!qobject_cast<WLayerSurface *>(target)) {
        m_shadowState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
        m_borderState = PersonalizationWindowContextV1::DecorationState::EnabledDefault;
    }

    connect(target, &WToplevelSurface::aboutToBeInvalidated, this, [this] {
        disconnect(m_manager, &PersonalizationManagerInterfaceV1::windowContextCreated, this, nullptr);
    });

    auto update = [this](PersonalizationWindowContextV1 *context) {
        assert(context);

        if (WSurface::fromHandle(context->surface()) != m_target->surface()) {
            return;
        }

        connect(context,
                &PersonalizationWindowContextV1::backgroundTypeChanged,
                this,
                [this, context] {
                    setBackgroundType(static_cast<Personalization::BackgroundType>(context->backgroundType()));
                });
        connect(context,
                &PersonalizationWindowContextV1::cornerRadiusChanged,
                this,
                [this, context] {
                    setCornerRadius(context->cornerRadius());
                });

        connect(context, &PersonalizationWindowContextV1::shadowChanged, this, [this, context] {
            setShadow(context->shadow());
        });

        connect(context, &PersonalizationWindowContextV1::borderChanged, this, [this, context] {
            setBorder(context->border());
        });

        connect(context,
                &PersonalizationWindowContextV1::windowStateChanged,
                this,
                [this, context] {
                    setWindowStates(context->states());
                });

        connect(context, &PersonalizationWindowContextV1::shadowStateChanged, this, [this, context] {
            setShadowState(context->shadowState());
        });

        connect(context, &PersonalizationWindowContextV1::borderStateChanged, this, [this, context] {
            setBorderState(context->borderState());
        });

        setBackgroundType(static_cast<Personalization::BackgroundType>(context->backgroundType()));
        setCornerRadius(context->cornerRadius());
        setShadow(context->shadow());
        setBorder(context->border());
        setWindowStates(context->states());
        setShadowState(context->shadowState());
        setBorderState(context->borderState());
    };

    connect(m_manager, &PersonalizationManagerInterfaceV1::windowContextCreated, this, update);

    if (auto *context = PersonalizationWindowContextV1::getWindowContext(m_target->surface())) {
        update(context);
    }
}

void Personalization::resetProperties()
{
    setBackgroundType(Personalization::BackgroundType::Normal);
    setCornerRadius(0);
    setShadow({});
    setBorder({});
    setWindowStates({});
}

SurfaceWrapper *Personalization::surfaceWrapper() const
{
    return qobject_cast<SurfaceWrapper*>(parent());
}

Personalization::BackgroundType Personalization::backgroundType() const
{
    return m_backgroundType;
}

bool Personalization::noTitlebar() const
{
    if (qobject_cast<WXdgPopupSurface *>(m_target)) {
        return true;
    }

    return m_states.testFlag(PersonalizationWindowContextV1::NoTitleBar);
}

bool Personalization::shadowEnabled() const
{
    return m_shadowState != PersonalizationWindowContextV1::DecorationState::NotSet;
}

bool Personalization::borderEnabled() const
{
    return m_borderState != PersonalizationWindowContextV1::DecorationState::NotSet;
}

void Personalization::setBackgroundType(BackgroundType type)
{
    if (m_backgroundType == type)
        return;
    m_backgroundType = type;
    Q_EMIT backgroundTypeChanged();
}

void Personalization::setCornerRadius(int32_t radius)
{
    if (m_cornerRadius == radius)
        return;
    m_cornerRadius = radius;
    Q_EMIT cornerRadiusChanged();
}

void Personalization::setShadow(const Shadow &shadow)
{
    if (m_shadow == shadow)
        return;
    m_shadow = shadow;
    Q_EMIT shadowChanged();
}

void Personalization::setBorder(const Border &border)
{
    if (m_border == border)
        return;
    m_border = border;
    Q_EMIT borderChanged();
}

void Personalization::setWindowStates(PersonalizationWindowContextV1::WindowStates states)
{
    if (m_states == states)
        return;
    m_states = states;
    Q_EMIT windowStateChanged();
}

void Personalization::setShadowState(PersonalizationWindowContextV1::DecorationState state)
{
    if (m_shadowState == state)
        return;
    m_shadowState = state;
    Q_EMIT shadowStateChanged();
}

void Personalization::setBorderState(PersonalizationWindowContextV1::DecorationState state)
{
    if (m_borderState == state)
        return;
    m_borderState = state;
    Q_EMIT borderStateChanged();
}

void PersonalizationManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void PersonalizationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *PersonalizationManagerInterfaceV1::global() const
{
    return d->global();
}

QByteArrayView PersonalizationManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}
