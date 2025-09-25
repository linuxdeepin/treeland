// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlocksurface.h"
#include "private/wtoplevelsurface_p.h"
#include "woutput.h"
#include "wsurface.h"
#include "wtoplevelsurface.h"

#include <qwcompositor.h>
#include <qwsessionlockv1.h>
#include <limits>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WSessionLockSurfacePrivate(WSessionLockSurface *qq, qw_session_lock_surface_v1 *handle);
    ~WSessionLockSurfacePrivate();

    WWRAP_HANDLE_FUNCTIONS(qw_session_lock_surface_v1, wlr_session_lock_surface_v1)

    wl_client *waylandClient() const override {
        return nativeHandle()->resource->client;
    }

    // begin slot function
    // end slot function

    void init();
    void instantRelease() override;

    W_DECLARE_PUBLIC(WSessionLockSurface)

    WSurface *surface = nullptr;
    WOutput *output = nullptr;
};

WSessionLockSurfacePrivate::WSessionLockSurfacePrivate(WSessionLockSurface *qq, qw_session_lock_surface_v1 *handle)
    : WToplevelSurfacePrivate(qq)
{
    initHandle(handle);
}

WSessionLockSurfacePrivate::~WSessionLockSurfacePrivate()
{

}

void WSessionLockSurfacePrivate::init() {
    W_Q(WSessionLockSurface);
    handle()->set_data(this, q);

    Q_ASSERT(!q->surface());
    auto qsurface = qw_surface::from((*handle())->surface);
    surface = new WSurface(qsurface, q);
    surface->setAttachedData<WSessionLockSurface>(q);

    output = nativeHandle()->output ? WOutput::fromHandle(qw_output::from(nativeHandle()->output)) : nullptr;
}

void WSessionLockSurfacePrivate::instantRelease()
{
    W_Q(WSessionLockSurface);
    
    handle()->set_data(nullptr, nullptr);
    auto qsurface = qw_surface::from((*handle())->surface);
    qsurface->disconnect(q);
    if (!surface)
        return;
    surface->safeDeleteLater();
    surface = nullptr;
}

WSessionLockSurface::WSessionLockSurface(qw_session_lock_surface_v1 *handle, QObject *parent)
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

qw_session_lock_surface_v1 *WSessionLockSurface::handle() const
{
    W_DC(WSessionLockSurface);
    return d->handle();
}

wlr_session_lock_surface_v1 *WSessionLockSurface::nativeHandle() const
{
    W_DC(WSessionLockSurface);
    return d->nativeHandle();
}

WSessionLockSurface *WSessionLockSurface::fromHandle(qw_session_lock_surface_v1 *handle)
{
    return handle->get_data<WSessionLockSurface>();
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
    W_D(WSessionLockSurface);
    return handle()->configure(newSize.width(), newSize.height());
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
    return QRect(0, 0, d->nativeHandle()->current.width, d->nativeHandle()->current.height);
}

WAYLIB_SERVER_END_NAMESPACE
