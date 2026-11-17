// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "toplevelcapturesource.h"
#include "toplevelcaptureviewport.h"

#include "output/output.h"
#include "surface/surfacewrapper.h"

ToplevelCaptureSource::ToplevelCaptureSource(SurfaceWrapper *wrapper)
    : WExtImageCaptureSourceV1Impl(wrapper)
    , m_wrapper(wrapper)
{
}

WOutputViewport *ToplevelCaptureSource::createViewport()
{
    // Resolve the output per session: the window may have moved while no
    // client was capturing it.
    if (!m_wrapper || !m_wrapper->surfaceItem() || !m_wrapper->ownsOutput())
        return nullptr;
    return new ToplevelCaptureViewport(m_wrapper, m_wrapper->ownsOutput()->output());
}
