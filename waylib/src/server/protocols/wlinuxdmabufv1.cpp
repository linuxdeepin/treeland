// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlinuxdmabufv1.h"

#include "wayliblogging.h"

#include <qwdisplay.h>
#include <qwlinuxdmabufv1.h>
#include <qwrenderer.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

constexpr uint32_t LinuxDmabufV1Version = 4;

WLinuxDmabufV1::WLinuxDmabufV1(qw_renderer *renderer, QObject *parent)
    : QObject(parent)
    , m_renderer(renderer)
{
}

qw_linux_dmabuf_v1 *WLinuxDmabufV1::handle() const
{
    return nativeInterface<qw_linux_dmabuf_v1>();
}

QByteArrayView WLinuxDmabufV1::interfaceName() const
{
    return "zwp_linux_dmabuf_v1";
}

void WLinuxDmabufV1::create(WServer *server)
{
    if (m_handle)
        return;

    if (!m_renderer || !m_renderer->handle()) {
        qCWarning(lcWlLinuxDmabuf) << "Cannot create linux-dmabuf global without renderer";
        return;
    }

    const auto *formats = m_renderer->get_texture_formats(WLR_BUFFER_CAP_DMABUF);
    if (!formats) {
        qCInfo(lcWlLinuxDmabuf) << "Skipping linux-dmabuf global: renderer does not expose DMA-BUF texture formats";
        return;
    }

    const int drmFd = m_renderer->get_drm_fd();
    if (drmFd < 0) {
        qCInfo(lcWlLinuxDmabuf) << "Skipping linux-dmabuf global: renderer has no DRM fd";
        return;
    }

    m_handle = qw_linux_dmabuf_v1::create_with_renderer(*server->handle(),
                                                        LinuxDmabufV1Version,
                                                        *m_renderer);
    if (!m_handle) {
        qCWarning(lcWlLinuxDmabuf) << "Failed to create renderer-backed linux-dmabuf global";
        return;
    }

    qCInfo(lcWlLinuxDmabuf) << "Created renderer-backed linux-dmabuf global"
                            << "version" << LinuxDmabufV1Version
                            << "drm fd" << drmFd;
}

void WLinuxDmabufV1::destroy([[maybe_unused]] WServer *server)
{
    // wlroots owns this global and destroys it from the display destroy listener.
}

wl_global *WLinuxDmabufV1::global() const
{
    if (auto *native = handle())
        return native->handle()->global;

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
