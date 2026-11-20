// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutput.h"
#include "wglobal_p.h"

#include <wlr_all.h>
#include <QPointer>
#include <QQuickWindow>

WAYLIB_SERVER_BEGIN_NAMESPACE

class QWlrootsScreen;
class WBackend;
class WOutputLayout;

class Q_DECL_HIDDEN WOutputPrivate : public WObjectPrivate
{
public:
    WOutputPrivate(WOutput *qq, wlr_output *handle);
    ~WOutputPrivate() = default;

    static inline WOutputPrivate *get(WOutput *output) {
        return output->d_func();
    }

    inline wlr_output *handle() const {
        return m_handle;
    }

    inline QSize size() const {
        Q_ASSERT(handle());
        return QSize(handle()->width, handle()->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle()->transform);
    }

    W_DECLARE_PUBLIC(WOutput)
    bool forceSoftwareCursor = false;

    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WBackend *backend = nullptr;
    WOutputLayout *layout = nullptr;

private:
    // The backend owns this handle and destroys it after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_output *m_handle = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
