// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdialogmanagerv1.h"

#include "private/wglobal_p.h"
#include "wayliblogging.h"
#include "wxdgtoplevelsurface.h"

#include <qwdisplay.h>
#include <qwxdgdialogv1.h>
#include <qwxdgshell.h>

#include <QPointer>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgDialogManagerV1Private : public WObjectPrivate
{
public:
    WXdgDialogManagerV1Private(WXdgDialogManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline qw_xdg_wm_dialog_v1 *handle() const
    {
        return q_func()->nativeInterface<qw_xdg_wm_dialog_v1>();
    }

    void onNewDialog(wlr_xdg_dialog_v1 *dialog);

    W_DECLARE_PUBLIC(WXdgDialogManagerV1)
};

void WXdgDialogManagerV1Private::onNewDialog(wlr_xdg_dialog_v1 *nativeDialog)
{
    W_Q(WXdgDialogManagerV1);

    auto *qwToplevel = qw_xdg_toplevel::get(nativeDialog->xdg_toplevel);
    if (!qwToplevel) {
        qCWarning(lcWlXdgDialog) << "Failed to get xdg_toplevel from dialog";
        return;
    }

    auto *surface = WXdgToplevelSurface::fromHandle(qwToplevel);
    if (!surface) {
        qCWarning(lcWlXdgDialog) << "Failed to get WXdgToplevelSurface from" << qwToplevel;
        return;
    }

    auto *qwDialog = qw_xdg_dialog_v1::from(nativeDialog);
    if (!qwDialog) {
        qCWarning(lcWlXdgDialog) << "Failed to create qw_xdg_dialog_v1 wrapper";
        return;
    }

    qCDebug(lcWlXdgDialog) << "New xdg_dialog_v1 for surface:" << surface
                           << "initial modal:" << nativeDialog->modal;

    // Wrap in QPointer to guard against toplevel being destroyed before the dialog.
    // surface is a WXdgToplevelSurface* (QObject*), so QPointer safely nulls when destroyed.
    QPointer<WXdgToplevelSurface> surfaceGuard(surface);

    // Sync the initial modal state (client may have called set_modal before we connect)
    if (nativeDialog->modal && surfaceGuard)
        Q_EMIT q->surfaceModalChanged(surfaceGuard, true);

    // Track future set_modal / unset_modal calls. Both requests fire notify_set_modal.
    QObject::connect(qwDialog,
                     &qw_xdg_dialog_v1::notify_set_modal,
                     q,
                     [q, nativeDialog, surfaceGuard]() {
                         if (!surfaceGuard)
                             return;
                         qCDebug(lcWlXdgDialog) << "xdg_dialog_v1 modal changed:" << surfaceGuard
                                                << "->" << nativeDialog->modal;
                         Q_EMIT q->surfaceModalChanged(surfaceGuard, nativeDialog->modal);
                     });

    // When the dialog object is destroyed, reset modal if it was set.
    // Protocol: "If this object is destroyed before the related xdg_toplevel,
    // the compositor should unapply its effects."
    QObject::connect(qwDialog,
                     &qw_xdg_dialog_v1::before_destroy,
                     q,
                     [q, nativeDialog, surfaceGuard]() {
                         if (nativeDialog->modal && surfaceGuard) {
                             qCDebug(lcWlXdgDialog)
                                 << "xdg_dialog_v1 destroyed while modal, resetting:"
                                 << surfaceGuard;
                             Q_EMIT q->surfaceModalChanged(surfaceGuard, false);
                         }
                     });
}

WXdgDialogManagerV1::WXdgDialogManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WXdgDialogManagerV1Private(this))
{
}

WXdgDialogManagerV1::~WXdgDialogManagerV1() = default;

QByteArrayView WXdgDialogManagerV1::interfaceName() const
{
    return "xdg_wm_dialog_v1";
}

void WXdgDialogManagerV1::create(WServer *server)
{
    auto *wm = qw_xdg_wm_dialog_v1::create(*server->handle(), 1);
    m_handle = wm;

    connect(wm, &qw_xdg_wm_dialog_v1::notify_new_dialog, this, [this](wlr_xdg_dialog_v1 *dialog) {
        W_D(WXdgDialogManagerV1);
        d->onNewDialog(dialog);
    });
}

void WXdgDialogManagerV1::destroy([[maybe_unused]] WServer *server)
{
    // qw_xdg_wm_dialog_v1 is owned by the wl_display; no explicit cleanup needed.
}

wl_global *WXdgDialogManagerV1::global() const
{
    W_DC(WXdgDialogManagerV1);
    if (!d->handle())
        return nullptr;
    return d->handle()->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
