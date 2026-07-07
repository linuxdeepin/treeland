// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QPointer>

class SurfaceWrapper;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WXdgPopupSurface;
WAYLIB_SERVER_END_NAMESPACE

// Manages xdg_popup keyboard grab lifecycle for the compositor.
//
// When a client calls xdg_popup.grab(), wlroots installs keyboard/pointer/touch
// grabs on the seat. This manager tracks the grab state so the compositor can:
//   1. Move keyboard focus to popup surfaces when they gain focus capability
//      during an active popup grab.
//   2. Restore keyboard focus to the pre-grab toplevel when the grab ends.
//   3. Dismiss all popups on workspace switch and similar scenarios.
//
// Only xdg_popup keyboard grabs are tracked; drag grabs are excluded (IME grabs are not yet distinguished).
class PopupFocusManager : public QObject
{
    Q_OBJECT

public:
    explicit PopupFocusManager(WAYLIB_SERVER_NAMESPACE::WSeat *seat, QObject *parent = nullptr);
    ~PopupFocusManager() override;

    // Move keyboard focus to the given xdg_popup surface. If this is the first
    // popup during the current grab, the previously focused surface is saved
    // for later restoration. No-op if no popup grab is active.
    void giveFocus(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popupSurface);

    // Dismiss all active popup surfaces. No-op if no popup grab is active.
    void dismissAll();

Q_SIGNALS:
    // Emitted when all popups should be dismissed. ShellHandler connects
    // to this to close popup wrappers via closeSurface().
    void aboutToDismissAll();

private Q_SLOTS:
    void onKeyboardGrabBegin();
    void onKeyboardGrabEnd();

private:
    // TODO(rewine): Track per-seat popup grab state for multi-seat support.
    WAYLIB_SERVER_NAMESPACE::WSeat *m_seat = nullptr;
    bool m_hasPopupGrab = false;
    QPointer<SurfaceWrapper> m_savedFocusSurface;
};
