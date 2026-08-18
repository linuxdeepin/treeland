// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlocksurface.h"
#include "private/wtoplevelsurface_p.h"
#include "woutput.h"
#include "wscoplistener.h"
#include "wsurface.h"
#include "wtoplevelsurface.h"

#include <wlr_all.h>

#include <limits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WSessionLockSurfacePrivate(WSessionLockSurface *qq, wlr_session_lock_surface_v1 *handle);
    ~WSessionLockSurfacePrivate();

    inline wlr_session_lock_surface_v1 *handle() const {
        return m_handle;
    }

    wl_client *waylandClient() const override {
        return handle()->resource->client;
    }

    // begin slot function
    // end slot function

    void init();

    W_DECLARE_PUBLIC(WSessionLockSurface)

    WSurface *surface = nullptr;
    WOutput *output = nullptr;

private:
    // The session-lock owner destroys this handle after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_session_lock_surface_v1 *m_handle = nullptr;
};

WSessionLockSurfacePrivate::WSessionLockSurfacePrivate(WSessionLockSurface *qq, wlr_session_lock_surface_v1 *handle)
    : WToplevelSurfacePrivate(qq)
{
    Q_ASSERT(handle);
    handle->data = qq;
    m_handle = handle;
}

WSessionLockSurfacePrivate::~WSessionLockSurfacePrivate()
{

}

void WSessionLockSurfacePrivate::init() {
    W_Q(WSessionLockSurface);
    Q_ASSERT(!q->surface());
    surface = new WSurface(m_handle->surface);
    surface->setAttachedData<WSessionLockSurface>(q);

    output = handle()->output ? WOutput::fromHandle(handle()->output) : nullptr;
}

WSessionLockSurface::WSessionLockSurface(wlr_session_lock_surface_v1 *handle)
    : WToplevelSurface(*new WSessionLockSurfacePrivate(this, handle))
{
    d_func()->init();
}

WSessionLockSurface::~WSessionLockSurface()
{
    teardown();
    // Notify listeners while the object is still usable (its members are
    // alive during the destructor body).
    Q_EMIT beforeDestroy();
    // Owner rule: this object created the WSurface wrapper, release it.
    W_D(WSessionLockSurface);
    // Clear the reverse fromHandle() mapping. The lock destroys this wrapper
    // from the lock surface's destroy callback (or while the native handle
    // is still alive), so the handle is valid here.
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    delete d->surface;
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
    if (!handle)
        return nullptr;
    return static_cast<WSessionLockSurface*>(handle->data);
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

#include "moc_wsessionlocksurface.cpp"
