// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test application for treeland_keyboard_state_notify_manager_v1
// (treeland-keyboard-state-notify-unstable-v1).
// Creates a keyboard state watcher that monitors specified lock modifiers and prints
// their state transitions (locked/unlocked) to the console.
//
// Usage: test-key-notify --modifier <modifier_name>[,<modifier_name>...] [--watch locked|unlocked|all]
//   e.g. test-key-notify --modifier caps_lock,num_lock --watch locked
//        test-key-notify --modifier caps_lock --modifier num_lock,scroll_lock --watch all

#include "qwayland-treeland-keyboard-state-notify-unstable-v1.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QWaylandClientExtension>

static QString modifierName(uint32_t modifier)
{
    if (modifier == QtWayland::treeland_keyboard_state_watcher_v1::modifier_caps_lock)
        return QStringLiteral("Caps Lock");
    if (modifier == QtWayland::treeland_keyboard_state_watcher_v1::modifier_num_lock)
        return QStringLiteral("Num Lock");
    return QStringLiteral("Unknown(0x") + QString::number(modifier, 16) + ')';
}

class KeyboardStateWatcher
    : public QObject
    , public QtWayland::treeland_keyboard_state_watcher_v1
{
    Q_OBJECT
public:
    explicit KeyboardStateWatcher(struct ::treeland_keyboard_state_watcher_v1 *watcher,
                                   QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_keyboard_state_watcher_v1(watcher)
    {
    }

    ~KeyboardStateWatcher() override
    {
        destroy();
    }

Q_SIGNALS:
    void modifierLocked(uint32_t modifier);
    void modifierUnlocked(uint32_t modifier);

protected:
    void treeland_keyboard_state_watcher_v1_current_state(uint32_t modifier, uint32_t state) override
    {
        if (state == QtWayland::treeland_keyboard_state_watcher_v1::modifier_state_locked) {
            qWarning() << "current_state: [Locked]" << modifierName(modifier);
            return;
        }
        else if (state == QtWayland::treeland_keyboard_state_watcher_v1::modifier_state_unlocked) {
            qWarning() << "current_state: [Unlocked]" << modifierName(modifier);
        }
    }

    void treeland_keyboard_state_watcher_v1_state_changed(uint32_t modifier, uint32_t state) override
    {
        if (state == QtWayland::treeland_keyboard_state_watcher_v1::modifier_state_locked) {
            qWarning() << "state_changed: [Locked]" << modifierName(modifier);
            return;
        }
        else if (state == QtWayland::treeland_keyboard_state_watcher_v1::modifier_state_unlocked) {
            qWarning() << "state_changed: [Unlocked]" << modifierName(modifier);
        }
    }
};

class KeyboardStateNotifyManagerV1
    : public QWaylandClientExtensionTemplate<KeyboardStateNotifyManagerV1>
    , public QtWayland::treeland_keyboard_state_notify_manager_v1
{
    Q_OBJECT
public:
    static constexpr int InterfaceVersion = 1;

    explicit KeyboardStateNotifyManagerV1()
        : QWaylandClientExtensionTemplate<KeyboardStateNotifyManagerV1>(InterfaceVersion)
    {
    }

    KeyboardStateWatcher *createWatcher(struct ::wl_seat *seat = nullptr,
                                         QObject *parent = nullptr)
    {
        auto *raw = get_keyboard_state_watcher(seat);
        if (!raw)
            return nullptr;
        return new KeyboardStateWatcher(raw, parent);
    }
};

static bool parseModifier(const QString &name, uint32_t &out)
{
    if (name.compare(QLatin1String("caps_lock"), Qt::CaseInsensitive) == 0) {
        out = QtWayland::treeland_keyboard_state_watcher_v1::modifier_caps_lock;
        return true;
    }
    if (name.compare(QLatin1String("num_lock"), Qt::CaseInsensitive) == 0) {
        out = QtWayland::treeland_keyboard_state_watcher_v1::modifier_num_lock;
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Treeland Keyboard State Notify Test Client"));
    parser.addHelpOption();

    QCommandLineOption modifierOption(
        QStringLiteral("modifier"),
        QStringLiteral("Lock modifier to watch: caps_lock,num_lock. Can be repeated."),
        QStringLiteral("modifier-name"));
    parser.addOption(modifierOption);

    QCommandLineOption watchOption(
        QStringLiteral("watch"),
        QStringLiteral("Which transitions to watch: locked, unlocked, or all (default: all)."),
        QStringLiteral("mode"),
        QStringLiteral("all"));
    parser.addOption(watchOption);
    parser.process(app);

    uint32_t modifiers = 0;
    const auto modifierArgs = parser.values(modifierOption);
    if (modifierArgs.isEmpty()) {
        qCritical() << "No modifiers specified. Use --modifier <name> to specify lock modifiers to watch.";
        parser.showHelp(EXIT_FAILURE);
    }

    for (const auto &raw : modifierArgs) {
        for (const auto &name : raw.split(',')) {
            const auto trimmed = name.trimmed();
            if (trimmed.isEmpty())
                continue;

            uint32_t mod = 0;
            if (!parseModifier(trimmed, mod)) {
                qWarning() << "Unknown modifier:" << trimmed
                           << "(expected: caps_lock, num_lock)";
                continue;
            }
            modifiers |= mod;
        }
    }

    if (modifiers == 0) {
        qCritical() << "No valid modifiers to watch.";
        return EXIT_FAILURE;
    }

    QStringList watchingNames;
    if (modifiers & QtWayland::treeland_keyboard_state_watcher_v1::modifier_caps_lock)
        watchingNames << "Caps Lock";
    if (modifiers & QtWayland::treeland_keyboard_state_watcher_v1::modifier_num_lock)
        watchingNames << "Num Lock";
    qDebug() << "Watching modifiers:" << watchingNames.join(", ");

    auto *manager = new KeyboardStateNotifyManagerV1();

    QObject::connect(manager, &KeyboardStateNotifyManagerV1::activeChanged, manager, [&]() {
        if (!manager->isActive()) {
            qWarning() << "Protocol not available.";
            return;
        }

        qDebug() << "Protocol active, creating keyboard state watcher...";

        auto *watcher = manager->createWatcher(nullptr, manager);
        if (!watcher) {
            qCritical() << "Failed to create keyboard state watcher.";
            qApp->exit(EXIT_FAILURE);
            return;
        }

        watcher->set_modifiers(modifiers);

        const QString watchMode = parser.value(watchOption);
        uint32_t flags = 0;
        if (watchMode == QLatin1String("locked") || watchMode == QLatin1String("all"))
            flags |= QtWayland::treeland_keyboard_state_watcher_v1::watch_flag_locked;
        if (watchMode == QLatin1String("unlocked") || watchMode == QLatin1String("all"))
            flags |= QtWayland::treeland_keyboard_state_watcher_v1::watch_flag_unlocked;

        if (flags == 0) {
            qCritical() << "Invalid --watch value:" << watchMode << "(expected: locked, unlocked, all)";
            qApp->exit(EXIT_FAILURE);
            return;
        }

        watcher->set_flags(flags);
        watcher->apply();

        qDebug() << "Keyboard state watcher created. Press Ctrl+C to quit.";
    });

    return app.exec();
}

#include "main.moc"
