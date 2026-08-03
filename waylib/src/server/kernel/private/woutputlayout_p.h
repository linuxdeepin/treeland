// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "wglobal_p.h"

struct wlr_output_layout;

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputLayoutPrivate : public WWrapObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq);
    ~WOutputLayoutPrivate();

    void doAdd(WOutput *output);

    void instantRelease() override;

    static void handleDestroy(wl_listener *listener, void *data);
    static void handleChange(wl_listener *listener, void *data);

    W_DECLARE_PUBLIC(WOutputLayout)

    QList<WOutput*> outputs;
    wlr_output_layout *handle = nullptr;
    wl_listener destroy;
    wl_listener change;

    void updateImplicitSize();
    int implicitWidth { 0 };
    int implicitHeight { 0 };
};

WAYLIB_SERVER_END_NAMESPACE
