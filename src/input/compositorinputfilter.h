// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>

#include <QKeyEvent>

class SeatManager;
class WaylandServer;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WRelativePointerManagerV1;
WAYLIB_SERVER_END_NAMESPACE

class FocusController;
class IdleManager;
class CaptureManager;
class OutputManager;
class ShortcutManagerV3;
class KeyboardShortcutsInhibitManagerV1;

WAYLIB_SERVER_USE_NAMESPACE

class CompositorInputFilter : public WSeatEventFilter
{
    Q_OBJECT
public:
    CompositorInputFilter(WaylandServer *server, FocusController *focus,
                         IdleManager *idleManager, CaptureManager *captureManager,
                         OutputManager *outputManager);

    void installRelativePointer(WServer *server, SeatManager *seatManager);
    void setShortcutProtocols(ShortcutManagerV3 *shortcuts,
                              KeyboardShortcutsInhibitManagerV1 *inhibitor);
    WSeat *currentEventSeat() const { return m_currentEventSeat; }

Q_SIGNALS:
    void modifierKeyReleased(QKeyEvent *event);

private:
    void trackSeat(WSeat *seat);
    WRelativePointerManagerV1 *m_relativePointerManager = nullptr;
    bool beforeDisposeEvent(WSeat *seat, QWindow *window, QInputEvent *event) override;
    bool afterHandleEvent(WSeat *seat, WSurface *watched, QObject *shellObject,
                          QObject *eventObject, QInputEvent *event) override;
    bool unacceptedEvent(WSeat *seat, QWindow *window, QInputEvent *event) override;
    bool doGesture(QInputEvent *event);
    void handleLeftButtonStateChanged(const QInputEvent *event);
    void handleWhellValueChanged(const QInputEvent *event);

private:
    WaylandServer *m_server = nullptr;
    FocusController *m_focus = nullptr;
    IdleManager *m_idleManager = nullptr;
    CaptureManager *m_captureManager = nullptr;
    OutputManager *m_outputManager = nullptr;
    ShortcutManagerV3 *m_shortcutManager = nullptr;
    KeyboardShortcutsInhibitManagerV1 *m_keyboardShortcutsInhibitManagerV1 = nullptr;
    WSeat *m_currentEventSeat = nullptr;
};
