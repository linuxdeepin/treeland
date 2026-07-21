// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QRect>
#include <QString>
#include <memory>

#include <sys/types.h>

Q_MOC_INCLUDE(<woutput.h>)

class SurfaceEntry;
class ForeignToplevelManagerInterfaceV1;
class ForeignToplevelHandleV1Private;
struct wl_resource;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
class WSeat;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

class ForeignToplevelHandleV1 : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Maximized = 1,
        Minimized = 2,
        Activated = 4,
        Fullscreen = 8,
        Attention = 16,
    };
    Q_ENUM(State)
    Q_DECLARE_FLAGS(States, State)

    ~ForeignToplevelHandleV1() override;

    void set_title(const QString &title);
    void set_app_id(const QString &app_id);
    void set_pid(const pid_t pid);
    void set_maximized(bool maximized);
    void set_minimized(bool minimized);
    void set_activated(bool activated);
    void set_fullscreen(bool fullscreen);
    void set_attention(bool attention);
    void set_parent(ForeignToplevelHandleV1 *parent);

Q_SIGNALS:
    void requestMaximize(bool maximized);
    void requestMinimize(bool minimized);
    void requestActivate(WAYLIB_SERVER_NAMESPACE::WSeat *seat);
    void requestFullscreen(bool fullscreen, WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void requestClose();
    void rectangleChanged(WAYLIB_SERVER_NAMESPACE::WSurface *surface, const QRect &rect);

private:
    explicit ForeignToplevelHandleV1(ForeignToplevelManagerInterfaceV1 *manager,
                                     wl_resource *resource,
                                     SurfaceEntry *entry);

    wl_resource *resource() const;
    SurfaceEntry *entry() const;
    void clearEntry();
    void set_identifier(uint32_t identifier);
    uint32_t identifier() const;
    void send_done();
    void send_closed();
    void send_state();
    void output_enter(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void output_leave(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void send_output(WAYLIB_SERVER_NAMESPACE::WOutput *output, bool enter);

    std::unique_ptr<ForeignToplevelHandleV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1;
    friend class ForeignToplevelManagerInterfaceV1Private;
};
