// Copyright (C) 2023-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut.h"

#include <systemd/sd-daemon.h>

#include <QApplication>
#include <QDBusInterface>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(treelandShortcut, "daemon.shortcut", QtDebugMsg);

static const QMap<QString, QString> SpecialKeyMap = {
    { "minus", "-" },      { "equal", "=" },     { "brackertleft", "[" }, { "breckertright", "]" },
    { "backslash", "\\" }, { "semicolon", ";" }, { "apostrophe", "'" },   { "comma", "," },
    { "period", "." },     { "slash", "/" },     { "grave", "`" },
};

static const QMap<QString, QString> SpecialRequireShiftKeyMap = {
    { "exclam", "!" },    { "at", "@" },          { "numbersign", "#" }, { "dollar", "$" },
    { "percent", "%" },   { "asciicircum", "^" }, { "ampersand", "&" },  { "asterisk", "*" },
    { "parenleft", "(" }, { "parenright", ")" },  { "underscore", "_" }, { "plus", "+" },
    { "braceleft", "{" }, { "braceright", "}" },  { "bar", "|" },        { "colon", ":" },
    { "quotedbl", "\"" }, { "less", "<" },        { "greater", ">" },    { "question", "?" },
    { "asciitilde", "~" }
};

// from dtkcore util
QString getAppIdFromAbsolutePath(const QString &path)
{
    static QString desktopSuffix{ u8".desktop" };
    const auto &appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    if (!path.endsWith(desktopSuffix)
        || !std::any_of(appDirs.cbegin(), appDirs.constEnd(), [&path](const QString &dir) {
               return path.startsWith(dir);
           })) {
        return {};
    }

    auto tmp = path.chopped(desktopSuffix.size());
    auto components = tmp.split(QDir::separator(), Qt::SkipEmptyParts);
    auto location = std::find(components.cbegin(), components.cend(), "applications");
    if (location == components.cend()) {
        return {};
    }

    auto appId = QStringList{ location + 1, components.cend() }.join('-');
    return appId;
}

QString escapeToObjectPath(const QString &str)
{
    if (str.isEmpty()) {
        return "_";
    }

    auto ret = str;
    QRegularExpression re{ R"([^a-zA-Z0-9])" };
    auto matcher = re.globalMatch(ret);
    while (matcher.hasNext()) {
        auto replaceList = matcher.next().capturedTexts();
        replaceList.removeDuplicates();
        for (const auto &c : replaceList) {
            auto hexStr = QString::number(static_cast<uint>(c.front().toLatin1()), 16);
            ret.replace(c, QString{ R"(_%1)" }.arg(hexStr));
        }
    }
    return ret;
}

QString transFromDaemonAccelStr(const QString &accelStr)
{
    if (accelStr.isEmpty()) {
        return accelStr;
    }

    QString str(accelStr);

    str.remove("<").replace(">", "+").replace("Control", "Ctrl").replace("Super", "Meta");

    for (auto it = SpecialKeyMap.constBegin(); it != SpecialKeyMap.constEnd(); ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str;
        }
    }

    for (auto it = SpecialRequireShiftKeyMap.constBegin();
         it != SpecialRequireShiftKeyMap.constEnd();
         ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str.remove("Shift+");
        }
    }

    return str;
}

ShortcutManagerV1::ShortcutManagerV1()
    : QWaylandClientExtensionTemplate<ShortcutManagerV1>(2)
{
    connect(this, &ShortcutManagerV1::activeChanged, this, [this] {
        qCDebug(treelandShortcut) << "isActive:" << isActive();

        if (isActive()) {
            // TODO: Use a converter
            const QString configDir =
                QString("%1/deepin/dde-daemon/keybinding/")
                    .arg(QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first());
            const QString customIni = QString("%1/custom.ini").arg(configDir);

            auto updateShortcuts = [this, customIni] {
                m_customShortcuts.clear();

                QSettings custom(customIni, QSettings::IniFormat);
                for (auto group : custom.childGroups()) {
                    const QString &action =
                        custom.value(QString("%1/Action").arg(group)).toString();
                    const QString &accels = transFromDaemonAccelStr(
                        custom.value(QString("%1/Accels").arg(group)).toString());
                    ShortcutV1 *context =
                        new ShortcutV1(register_shortcut());
                        // context->bind_keys(accels);
                    connect(context, &ShortcutV1::shortcutHappended, this, [action] {
                        QProcess::startDetached(action);
                    });
                    m_customShortcuts.emplace_back(context);
                }
            };

            QFileSystemWatcher *watcher = new QFileSystemWatcher({ customIni }, this);
            connect(watcher, &QFileSystemWatcher::fileChanged, this, [updateShortcuts] {
                updateShortcuts();
            });

            updateShortcuts();

            QDir dir("/home/akari/UnionTech/treeland/src/treeland-shortcut/shortcuts");
            for (auto d : dir.entryInfoList(QDir::Filter::Files)) {
                qCInfo(treelandShortcut) << "Load shortcut:" << d.filePath();
                auto shortcut = new Shortcut(d.filePath());
                ShortcutV1 *context =
                    new ShortcutV1(register_shortcut());
                context->bind_keys(shortcut->shortcut());
                connect(context, &ShortcutV1::shortcutHappended, this, [shortcut] {
                    qCInfo(treelandShortcut) << "Shortcut happended: " << shortcut->shortcut();
                    shortcut->exec();
                });
                m_treelandShortcutContexts.emplace_back(context);
                m_treelandShortcuts.emplace_back(shortcut);
            }
        }
    });
}

ShortcutV1::ShortcutV1(struct ::treeland_shortcut_v1 *object)
    : QWaylandClientExtensionTemplate<ShortcutV1>(1)
    , QtWayland::treeland_shortcut_v1(object)
{
}

ShortcutV1::~ShortcutV1()
{
    destroy();
}

void ShortcutV1::treeland_shortcut_v1_activated()
{
    Q_EMIT shortcutHappended();
}

void ShortcutV1::treeland_shortcut_v1_bind_success(uint32_t binding_id)
{
    Q_UNUSED(binding_id)
    qCDebug(treelandShortcut) << "Shortcut bind success, binding id:" << binding_id;
}

void ShortcutV1::treeland_shortcut_v1_bind_failure(uint32_t reason)
{
    qCDebug(treelandShortcut) << "Shortcut bind failure, reason:" << reason;
}

Shortcut::Shortcut(const QString &path)
    : m_settings(QSettings(path, QSettings::IniFormat))
{
}

void Shortcut::exec()
{
    const QString &type = m_settings.value("Shortcut/Type").toString();

    if (type == "Exec") {
        const QString &typeExec = m_settings.value("Type.Exec/Exec").toString();
        const QString &typeArgs = m_settings.value("Type.Exec/Args").toString();
        QProcess::startDetached(typeExec, typeArgs.split(" "));
    }

    if (type == "DBus") {
        const QString &service = m_settings.value("Type.DBus/Service").toString();
        const QString &path = m_settings.value("Type.DBus/Path").toString();
        const QString &interface = m_settings.value("Type.DBus/Interface").toString();
        const QString &method = m_settings.value("Type.DBus/Method").toString();

        QDBusInterface dbus(service, path, interface);
        dbus.asyncCall(method);
    }

    if (type == "Action") { }

    if (type == "Application") {
        const QString &service = u8"org.desktopspec.ApplicationManager1";
        const QString &prefixPath = u8"/org/desktopspec/ApplicationManager1/";
        const QString &interface = u8"org.desktopspec.ApplicationManager1.Application";
        const QString &appPath = m_settings.value("Type.Application/App").toString();
        const QString &appId = getAppIdFromAbsolutePath(appPath);
        const QString &objectPath = prefixPath + escapeToObjectPath(appId);

        QDBusInterface dbus(service, objectPath, interface);
        dbus.asyncCall(u8"Launch", "", QStringList{}, QVariantMap{});
    }
}

QString Shortcut::shortcut()
{
    return m_settings.value("Shortcut/Shortcut").toString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    new ShortcutManagerV1;

    sd_notify(0, "READY=1");

    return app.exec();
}
