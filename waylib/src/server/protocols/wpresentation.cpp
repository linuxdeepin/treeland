// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wpresentation.h"

#include "wayliblogging.h"
#include "woutput.h"
#include "wsurface.h"

#include <wlr_all.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

constexpr uint32_t PresentationVersion = 2;

WPresentation::WPresentation(wlr_backend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
}

wlr_presentation *WPresentation::handle() const
{
    return nativeInterface<wlr_presentation>();
}

QByteArrayView WPresentation::interfaceName() const
{
    return "wp_presentation";
}

void WPresentation::surfaceTexturedOnOutput(WSurface *surface, WOutput *output)
{
    auto *native = handle();
    if (!native || !surface || !surface->handle() || !output || !output->handle())
        return;

    wlr_presentation_surface_textured_on_output(surface->handle(), output->handle());
    qCDebug(lcWlPresentation) << "Marked surface textured on output"
                              << "surface" << surface
                              << "wlrSurface" << surface->handle()
                              << "output" << output
                              << "wlrOutput" << output->handle();
}

void WPresentation::create(WServer *server)
{
    if (m_handle)
        return;

    if (!m_backend) {
        qCWarning(lcWlPresentation) << "Cannot create presentation-time global without backend";
        return;
    }

    m_handle = wlr_presentation_create(server->handle(), m_backend,
                                       PresentationVersion);
    if (!m_handle) {
        qCWarning(lcWlPresentation) << "Failed to create presentation-time global";
        return;
    }

    qCInfo(lcWlPresentation) << "Created presentation-time global"
                             << "version" << PresentationVersion;
}

void WPresentation::destroy([[maybe_unused]] WServer *server)
{
    // wlroots owns this global and destroys it from the display destroy listener.
}

wl_global *WPresentation::global() const
{
    if (auto *native = handle())
        return native->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
