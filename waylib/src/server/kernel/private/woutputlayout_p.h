// Copyright (C) 2023-2026 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "wglobal_p.h"

struct wlr_output_layout;
struct wl_display;

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutPrivate : public WWrapObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq);
    ~WOutputLayoutPrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_output_layout)

    // Create the wlr_output_layout and register it as the native handle.
    // The layout is owned by this object; its destroy event is handled by
    // WWrapObjectPrivate (onNativeDestroy -> safeDeleteLater), mirroring the
    // prior qw_object on_destroy semantics. The change event is re-emitted as
    // WOutputLayout::layoutChanged().
    void init(wl_display *display);

    void doAdd(WOutput *output);

    void instantRelease() override {
        m_changeListener.remove();
    }

    W_DECLARE_PUBLIC(WOutputLayout)

    QList<WOutput*> outputs;

    WScopedListener m_changeListener;

    void updateImplicitSize();
    int implicitWidth { 0 };
    int implicitHeight { 0 };
};

WAYLIB_SERVER_END_NAMESPACE
