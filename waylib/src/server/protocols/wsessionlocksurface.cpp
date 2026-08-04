// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlocksurface.h"
#include "private/wtoplevelsurface_p.h"
#include "woutput.h"
#include "wsurface.h"
#include "wtoplevelsurface.h"

#include <wlr/types/wlr_session_lock_v1.h>
#include <limits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WSessionLockSurfacePrivate(WSessionLockSurface *qq, wlr_session_lock_surface_v1 *handle);
    ~WSessionLockSurfacePrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_session_lock_surface_v1)

    wl_client *waylandClient() const override {
        return handle()->resource->client;
    }

    // begin slot function
    // end slot function

    void init();
    void instantRelease() override;

    W_DECLARE_PUBLIC(WSessionLockSurface)

    WSurface *surface = nullptr;
    WOutput *output = nullptr;
};

WSessionLockSurfacePrivate::WSessionLockSurfacePrivate(WSessionLockSurface *qq, wlr_session_lock_surface_v1 *handle)
    : WToplevelSurfacePrivate(qq)
{
    initNativeHandle(handle, &handle->events.destroy);
}

WSessionLockSurfacePrivate::~WSessionLockSurfacePrivate()
{

}

void WSessionLockSurfacePrivate::init() {
    W_Q(WSessionLockSurface);

    Q_ASSERT(!q->surface());
    surface = new WSurface(handle()->surface, q);
    surface->setAttachedData<WSessionLockSurface>(q);

    output = handle()->output ? WOutput::fromHandle(handle()->output) : nullptr;
}

void WSessionLockSurfacePrivate::instantRelease()
{
    if (!surface)
        return;
    surface->safeDeleteLater();
    surface = nullptr;
}

WSessionLockSurface::WSessionLockSurface(wlr_session_lock_surface_v1 *handle, QObject *parent)
    : WToplevelSurface(*new WSessionLockSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WSessionLockSurface::~WSessionLockSurface()
 {
    
}

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
    return static_cast<WSessionLockSurface*>(WWrapObjectPrivate::fromNativeHandle(handle));
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
    return wlr_session_lock_surface_v1_configure(d_func()->handle(), newSize.width(), newSize.height());
}

void WSessionLockSurface::resize(const QSize &size)
{
    configureSize(size);
}

bool WSessionLockSurface::checkNewSize(const QSize &size, QSize *clippedSize)
{
    // Session lock surfaces should accept any size, as they need to cover the entire output
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
