// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessioninputchannel.h"
#include "sessioninputcontext.h"
#include "session.h"
#include "sessionseatmanager.h"
#include "desktop/desktopenvironment.h"
#include "core/waylandserver.h"
#include "input/inputmanager.h"
#include "input/gesturemanager.h"
#include "input/compositorinputfilter.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/shortcut/shortcutmanager.h"
#include <winputdevice.h>
#include <wlr_all.h>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>

SessionInputChannel::SessionInputChannel(Session *owner, WaylandServer *runtime)
    : SessionComponent(owner)
    , m_runtime(runtime)
    , m_input(owner->desktop()->input())
    , m_gestures(new GestureManager(this))
    , m_shortcuts(new ShortcutController(this, m_gestures))
    , m_filter(runtime->createInputFilter(new SessionInputContext(owner, runtime, this),
                                          m_gestures, m_shortcuts, this))
{
    m_shortcutProtocol = new ShortcutManagerV3(this, m_shortcuts,
        [owner] { return owner->seatManager()->seats(); });
    owner->attachInterface(m_shortcutProtocol);
    m_runtime->configureInputFilter(m_filter, m_shortcutProtocol);
    m_filter->setEnabled(false);
    m_filter->setAdmission([this](WSeat *seat, QInputEvent *event) { return admitEvent(seat, event); });
    m_input->setGestureManager(m_gestures);
}

SessionInputChannel::~SessionInputChannel()
{
    deactivate();
    delete m_filter;
    // Controllers unregister gestures during destruction, so the recognizer
    // must outlive them independently of QObject child insertion order.
    delete m_shortcuts;
    delete m_gestures;
}

CompositorInputFilter *SessionInputChannel::filter() const { return m_filter; }
GestureManager *SessionInputChannel::gestures() const { return m_gestures; }
ShortcutController *SessionInputChannel::shortcuts() const { return m_shortcuts; }
ShortcutManagerV3 *SessionInputChannel::shortcutProtocol() const { return m_shortcutProtocol; }

void SessionInputChannel::activate()
{
    m_gestures->cancel();
    m_suppressedKeys.clear();
    m_suppressedButtons = QGuiApplication::mouseButtons();
    m_touchStarted = false;
    // Held physical keys belong to the desktop that received their press.
    // Hide them from the new client's enter state until the real release.
    for (auto *seat : session()->seatManager()->seats()) {
        seat->clearFilteredKeys();
        for (auto *device : seat->deviceList()) {
            if (device->type() != WInputDevice::Type::Keyboard)
                continue;
            auto *keyboard = wlr_keyboard_from_input_device(device->handle());
            if (!keyboard)
                continue;
            for (size_t i = 0; i < keyboard->num_keycodes; ++i) {
                const auto key = keyboard->keycodes[i];
                m_suppressedKeys[seat].insert(key);
                seat->addFilteredKey(key);
            }
        }
    }
    m_runtime->configureInputFilter(m_filter, m_shortcutProtocol);
    m_filter->setEnabled(true);
    m_input->activate();
}

void SessionInputChannel::deactivate()
{
    m_filter->setEnabled(false);
    m_gestures->cancel();
    if (m_input)
        m_input->deactivate();
    if (m_shortcutProtocol)
        m_shortcutProtocol->cancelCapture();
    m_suppressedKeys.clear();
    m_suppressedButtons = Qt::NoButton;
    m_touchStarted = false;
}

bool SessionInputChannel::admitEvent(WSeat *seat, QInputEvent *event)
{
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        const auto *key = static_cast<QKeyEvent *>(event);
        const auto code = key->nativeVirtualKey();
        auto &suppressed = m_suppressedKeys[seat];
        if (suppressed.contains(code)) {
            if (event->type() == QEvent::KeyRelease && !key->isAutoRepeat()) {
                suppressed.remove(code);
                seat->removeFilteredKey(code);
            }
            return false;
        }
        // Do not let a modifier held in the previous desktop turn a fresh
        // key into a shortcut in this desktop. Drain until all keys release.
        if (!suppressed.isEmpty()) {
            if (event->type() == QEvent::KeyPress && code) {
                suppressed.insert(code);
                seat->addFilteredKey(code);
            }
            return false;
        }
    }
    if (event->type() == QEvent::MouseButtonPress)
        m_suppressedButtons &= ~Qt::MouseButtons(static_cast<QMouseEvent *>(event)->button());
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto button = static_cast<QMouseEvent *>(event)->button();
        if (m_suppressedButtons.testFlag(button)) {
            m_suppressedButtons &= ~Qt::MouseButtons(button);
            return false;
        }
    }
    if (event->type() == QEvent::MouseMove && m_suppressedButtons != Qt::NoButton)
        return false;
    if (event->type() == QEvent::TouchBegin)
        m_touchStarted = true;
    else if (event->type() == QEvent::TouchUpdate && !m_touchStarted)
        return false;
    else if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        const bool started = m_touchStarted;
        m_touchStarted = false;
        return started;
    }
    return true;
}
