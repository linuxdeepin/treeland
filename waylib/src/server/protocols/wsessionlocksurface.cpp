// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlocksurface.h"
#include "private/wtoplevelsurface_p.h"
#include "woutput.h"
#include "wsurface.h"

#include <limits>

extern "C" {
#include <wlr/types/wlr_session_lock_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using LockSurfaceRegistry = QHash<const wlr_session_lock_surface_v1 *, WSessionLockSurface *>;
Q_GLOBAL_STATIC(LockSurfaceRegistry, s_lockSurfaces)

class Q_DECL_HIDDEN WSessionLockSurfacePrivate : public WToplevelSurfacePrivate
{
public:
    WSessionLockSurfacePrivate(WSessionLockSurface *qq, wlr_session_lock_surface_v1 *handle)
        : WToplevelSurfacePrivate(qq)
        , surfaceHandle(handle)
    {
        Q_ASSERT(surfaceHandle);
        Q_ASSERT(!s_lockSurfaces->contains(surfaceHandle));
        s_lockSurfaces->insert(surfaceHandle, qq);
    }

    inline wlr_session_lock_surface_v1 *handle() const { return surfaceHandle; }

    wl_client *waylandClient() const override
    {
        return surfaceHandle ? surfaceHandle->resource->client : nullptr;
    }

    void init();
    void instantRelease() override;

    W_DECLARE_PUBLIC(WSessionLockSurface)

    wlr_session_lock_surface_v1 *surfaceHandle = nullptr;
    WNativeListener destroyListener;
    WSurface *surface = nullptr;
    WOutput *output = nullptr;
};

void WSessionLockSurfacePrivate::init()
{
    W_Q(WSessionLockSurface);
    surface = new WSurface(handle()->surface, q);
    surface->setAttachedData<WSessionLockSurface>(q);
    output = handle()->output ? WOutput::fromHandle(handle()->output) : nullptr;
    destroyListener.connect(&handle()->events.destroy, [q](void *) {
        q->safeDeleteLater();
    });
}

void WSessionLockSurfacePrivate::instantRelease()
{
    if (surfaceHandle) {
        destroyListener.disconnect();
        s_lockSurfaces->remove(surfaceHandle);
        surfaceHandle = nullptr;
    }
    if (surface) {
        surface->safeDeleteLater();
        surface = nullptr;
    }
}

WSessionLockSurface::WSessionLockSurface(wlr_session_lock_surface_v1 *handle, QObject *parent)
    : WToplevelSurface(*new WSessionLockSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WSessionLockSurface::~WSessionLockSurface() = default;

bool WSessionLockSurface::hasCapability(Capability cap) const
{
    switch (cap) {
        using enum Capability;
    case Focus:
        return true;
    case Activate:
    case Maximized:
    case FullScreen:
    case Resize:
        return false;
    default:
        break;
    }
    Q_UNREACHABLE();
}

wlr_session_lock_surface_v1 *WSessionLockSurface::handle() const
{
    W_DC(WSessionLockSurface);
    return d->handle();
}

WSessionLockSurface *WSessionLockSurface::fromHandle(wlr_session_lock_surface_v1 *handle)
{
    return s_lockSurfaces->value(handle);
}

WSessionLockSurface *WSessionLockSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WSessionLockSurface>();
}

WSurface *WSessionLockSurface::surface() const
{
    W_DC(WSessionLockSurface);
    return d->surface;
}

WOutput *WSessionLockSurface::output() const
{
    W_DC(WSessionLockSurface);
    return d->output;
}

int WSessionLockSurface::keyboardFocusPriority() const
{
    return std::numeric_limits<int>::max();
}

uint32_t WSessionLockSurface::configureSize(const QSize &newSize)
{
    return wlr_session_lock_surface_v1_configure(handle(), newSize.width(), newSize.height());
}

void WSessionLockSurface::resize(const QSize &size)
{
    configureSize(size);
}

bool WSessionLockSurface::checkNewSize(const QSize &size, QSize *clippedSize)
{
    if (clippedSize)
        *clippedSize = size;
    return true;
}

QRect WSessionLockSurface::getContentGeometry() const
{
    W_DC(WSessionLockSurface);
    return QRect(0, 0, d->handle()->current.width, d->handle()->current.height);
}

WAYLIB_SERVER_END_NAMESPACE
