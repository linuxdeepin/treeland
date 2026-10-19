/***************************************************************************
 * Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
 * Copyright (c) 2015-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 * Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ***************************************************************************/

#include "sessionmodel.h"

#include <DDesktopEntry>

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcessEnvironment>
#include <QSet>
#include <QVector>

namespace {
    QStringList sessionFiles(const QStringList &dirPaths)
    {
        QStringList files;
        QSet<QString> taken;
        for (const auto &dirPath : dirPaths) {
            QDir dir(dirPath);
            dir.setNameFilters(QStringList() << QStringLiteral("*.desktop"));
            dir.setFilter(QDir::Files);
            for (const QString &entry : dir.entryList()) {
                if (taken.contains(entry))
                    continue;
                taken.insert(entry);
                files += dir.absoluteFilePath(entry);
            }
        }
        return files;
    }

    // TryExec names a program that must be available for the session to be
    // offered: an absolute path, or a name looked up in $PATH. An empty
    // TryExec does not restrict the session.
    bool tryExecAllowed(const QString &tryExec)
    {
        if (tryExec.isEmpty())
            return true;

        QFileInfo fi(tryExec);
        if (fi.isAbsolute())
            return fi.exists() && fi.isExecutable();

        const QStringList pathList = QProcessEnvironment::systemEnvironment()
                                         .value(QStringLiteral("PATH"))
                                         .split(QLatin1Char(':'));
        for (const QString &path : pathList) {
            fi.setFile(QDir(path), tryExec);
            if (fi.exists() && fi.isExecutable())
                return true;
        }
        return false;
    }
}

SessionModel::SessionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // refresh everytime a file is changed, added or removed
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SessionModel::reload);
}

void SessionModel::setSessionDirs(const QStringList &waylandSessionDirs,
                                  const QStringList &x11SessionDirs)
{
    m_watcher->removePaths(m_waylandSessionDirs);
    m_watcher->removePaths(m_x11SessionDirs);
    m_watcher->addPaths(waylandSessionDirs);
    m_watcher->addPaths(x11SessionDirs);

    m_waylandSessionDirs = waylandSessionDirs;
    m_x11SessionDirs = x11SessionDirs;

    reload();
}

void SessionModel::setLastSession(const QString &filePath)
{
    m_lastSession = filePath;

    const int index = indexOfSession(filePath);
    if (index >= 0)
        setCurrentIndex(index);
}

void SessionModel::reload()
{
    // Recheck for flag to show Wayland sessions
    const bool dri_active = QFileInfo::exists(QStringLiteral("/dev/dri"));

    // Keep the session the user selected across directory changes, e.g. when
    // a package installs or removes a session file.
    QString currentSession;
    if (m_currentIndex >= 0 && m_currentIndex < m_sessions.size())
        currentSession = m_sessions.at(m_currentIndex).filePath;

    beginResetModel();
    m_sessions.clear();
    m_displayNames.clear();
    m_currentIndex = 0;
    if (dri_active)
        populate(true, m_waylandSessionDirs);
    populate(false, m_x11SessionDirs);

    int index = indexOfSession(currentSession);
    if (index < 0)
        index = indexOfSession(m_lastSession);
    if (index >= 0)
        m_currentIndex = index;

    endResetModel();
    Q_EMIT currentIndexChanged(m_currentIndex);
}

int SessionModel::indexOfSession(const QString &filePath) const
{
    if (filePath.isEmpty())
        return -1;

    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).filePath == filePath)
            return i;
    }
    return -1;
}

QHash<int, QByteArray> SessionModel::roleNames() const
{
    // set role names
    QHash<int, QByteArray> roleNames;
    roleNames[DirectoryRole] = QByteArrayLiteral("directory");
    roleNames[FileRole] = QByteArrayLiteral("file");
    roleNames[NameRole] = QByteArrayLiteral("name");
    roleNames[ExecRole] = QByteArrayLiteral("exec");
    roleNames[CommentRole] = QByteArrayLiteral("comment");

    return roleNames;
}

void SessionModel::setCurrentIndex(int index)
{
    if (m_currentIndex != index) {
        m_currentIndex = index;
        Q_EMIT currentIndexChanged(index);
    }
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sessions.length());
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_sessions.count())
        return QVariant();

    // get session
    const SessionEntry &session = m_sessions.at(index.row());

    // return correct value
    switch (role) {
    case DirectoryRole:
        return QFileInfo(session.filePath).absolutePath();
    case FileRole:
        return session.filePath;
    case NameRole:
        if (m_displayNames.count(session.displayName) > 1 && session.wayland)
            return tr("%1 (Wayland)").arg(session.displayName);
        return session.displayName;
    case ExecRole:
        return session.exec;
    case CommentRole:
        return session.comment;
    default:
        break;
    }

    // return empty value
    return QVariant();
}

void SessionModel::populate(bool wayland, const QStringList &dirPaths)
{
    for (const QString &filePath : sessionFiles(dirPaths)) {
        // read session
        const Dtk::Core::DDesktopEntry entry(filePath);
        if (entry.status() != Dtk::Core::DDesktopEntry::NoError)
            continue;

        SessionEntry session;
        session.wayland = wayland;
        session.filePath = filePath;
        session.displayName = entry.name();
        session.exec = entry.rawValue(QStringLiteral("Exec"));
        session.comment = entry.comment();
        session.tryExec = entry.rawValue(QStringLiteral("TryExec"));

        if (!tryExecAllowed(session.tryExec))
            continue;

        // add to sessions list
        m_displayNames.append(session.displayName);
        if (session.displayName == QStringLiteral("Treeland"))
            m_sessions.prepend(session);
        else
            m_sessions.push_back(session);
    }
}
