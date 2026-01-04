// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <qwaylandclientextension.h>
#include <qapplication.h>

#include "qwayland-kde-keystate.h"

class KeyStateV5
    : public QWaylandClientExtensionTemplate<KeyStateV5>
    , public QtWayland::org_kde_kwin_keystate
{
    Q_OBJECT
public:
    explicit KeyStateV5()
        : QWaylandClientExtensionTemplate<KeyStateV5>(5)
    {

    }

    static QString keyToString(uint32_t key) {
        switch (static_cast<QtWayland::org_kde_kwin_keystate::key>(key)) {
            case key_capslock: return "CapsLock";
            case key_numlock: return "NumLock";
            case key_scrolllock: return "ScrollLock";
            case key_alt: return "Alt";
            case key_control: return "Control";
            case key_shift: return "Shift";
            case key_meta: return "Meta";
            case key_altgr: return "AltGr";
            default: return "Unknown:" + QString::number(key);
        }
    }

    static QString stateToString(uint32_t state) {
        switch (static_cast<QtWayland::org_kde_kwin_keystate::state>(state)) {
            case state_unlocked: return "Unlocked";
            case state_latched: return "Latched";
            case state_locked: return "Locked";
            case state_pressed: return "Pressed";
            default: return "Unknown:" + QString::number(state);
        }
    }

    void org_kde_kwin_keystate_stateChanged(uint32_t key, uint32_t state) override
    {
        qInfo() << "stateChanged: key =" << keyToString(key) << ", state =" << stateToString(state);
    }
public Q_SLOTS:
    void onActiveChanged() {
        if (isActive()) {
            qInfo() << "KeyStateV5 is active";
            qInfo() << "fetching key states";
            fetchStates();
        }
    };
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    KeyStateV5 keyState;
    keyState.setParent(&app);
    QObject::connect(&keyState, &KeyStateV5::activeChanged,
                     &keyState, &KeyStateV5::onActiveChanged,
                     Qt::QueuedConnection);
    app.exec();
    return 0;
}

#include "main.moc"
