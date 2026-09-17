// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wlr_fwd.h>
#include <wtoplevelsurface.h>

#include <QColor>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QQmlEngine>
#include <QString>

#include <memory>
#include <optional>

#include "common/windowdecorations.h"

class SurfaceWrapper;

WAYLIB_SERVER_USE_NAMESPACE

class DecorationManagerInterfaceV1;
class DecorationContextV1;

/**
 * @brief Per-window adapter between treeland_decoration_context_v1 and SurfaceWrapper.
 *
 * Attached to every toplevel SurfaceWrapper in Helper::onSurfaceWrapperAdded.
 * When a decoration context for the wrapped surface appears, its corner
 * radius / shadow / border / titlebar settings are applied to the wrapper.
 * The old personalization module's Personalization class is frozen; new
 * protocol support lives here.
 */
class Decoration : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    Decoration(WToplevelSurface *target, DecorationManagerInterfaceV1 *manager, SurfaceWrapper *parent);

    SurfaceWrapper *surfaceWrapper() const;

    /// Whether a decoration context has overridden the titlebar visibility.
    bool hasTitlebarOverride() const { return m_titlebarOverridden; }
    /// The titlebar override value: true = hide, false = show.
    bool titlebarHidden() const { return m_noTitleBar; }

Q_SIGNALS:
    /// Emitted when the titlebar override changes (or comes into effect).
    /// Helper re-arbitrates SurfaceWrapper::noTitleBar on this signal.
    void titlebarOverrideChanged();

private:
    void applyContext(DecorationContextV1 *context);
    void resetProperties();

    QPointer<WToplevelSurface> m_target;
    DecorationManagerInterfaceV1 *m_manager = nullptr;
    bool m_titlebarOverridden = false;
    bool m_noTitleBar = false;
};

/**
 * @brief Server implementation of treeland_decoration_manager_v1.
 *
 * Global factory that creates per-surface treeland_decoration_context_v1
 * objects. Each context lets the client override the compositor-drawn
 * server-side decorations (SSD) of one toplevel surface: corner radius,
 * shadow, border and titlebar visibility.
 *
 * Requests on a context are applied immediately; the compositor may clamp
 * out-of-range values to its supported bounds or ignore a set_* request on a
 * surface that has not negotiated SSD (per the protocol spec).
 */
class DecorationManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit DecorationManagerInterfaceV1(QObject *parent = nullptr);
    ~DecorationManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    /// A new per-surface decoration context was created.
    void contextCreated(DecorationContextV1 *context);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    friend class DecorationManagerInterfaceV1Private;
    struct Private;
    std::unique_ptr<Private> d;
};

/**
 * @brief Per-surface SSD customization context.
 *
 * Holds the four per-window override properties (corner radius, shadow,
 * border, titlebar mode) with a "set or default" semantics: a property that
 * was never set keeps its compositor default, tracked via std::optional
 * (nullopt = unset). The context becomes inert when its wl_surface is
 * destroyed; requests on an inert context are ignored.
 */
class DecorationContextV1 : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    DecorationContextV1(wl_resource *resource, wlr_surface *surface, QObject *parent = nullptr);
    ~DecorationContextV1() override;

    wl_resource *resource() const;
    wlr_surface *surface() const;

    // std::optional encodes the "set or default" semantics: nullopt means the
    // client never sent the request (compositor default applies), an engaged
    // value (including 0 / radius 0) means an explicit client override.
    std::optional<int32_t> cornerRadius() const;
    std::optional<Shadow> shadow() const;
    std::optional<Border> border() const;
    std::optional<uint32_t> titlebarMode() const;

    /// Find the live context for @p surface (nullptr if none).
    static DecorationContextV1 *forSurface(wlr_surface *surface);

Q_SIGNALS:
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void titlebarModeChanged();

private:
    friend class DecorationContextV1Private;
    std::unique_ptr<class DecorationContextV1Private> d;
};
