// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wextimagecapturesourcev1impl.h>

#include <QPointer>

WAYLIB_SERVER_USE_NAMESPACE

class SurfaceWrapper;

// Per-request foreign toplevel capture source. The capture pipeline belongs to
// the requesting client: waylib creates it in start() and destroys it in the
// last stop(), instead of keeping it alive until the toplevel disappears.
class ToplevelCaptureSource final : public WExtImageCaptureSourceV1Impl
{
public:
    explicit ToplevelCaptureSource(SurfaceWrapper *wrapper);

protected:
    WOutputViewport *createViewport() override;

private:
    QPointer<SurfaceWrapper> m_wrapper;
};
