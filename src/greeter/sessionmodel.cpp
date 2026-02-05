/***************************************************************************
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

#include <Configuration.h>

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcessEnvironment>
#include <QVector>

using namespace DDM;

SessionModel::SessionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Check for flag to show Wayland sessions
    bool dri_active = QFileInfo::exists(QStringLiteral("/dev/dri"));

    // initial population
    beginResetModel();
    if (dri_active)
        populate(Session::WaylandSession, mainConfig.Wayland.SessionDir.get());
    populate(Session::X11Session, mainConfig.X11.SessionDir.get());
    endResetModel();
    Q_EMIT currentIndexChanged(m_currentIndex);

    // refresh everytime a file is changed, added or removed
    QFileSystemWatcher *watcher = new QFileSystemWatcher(this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, [this]() {
        // Recheck for flag to show Wayland sessions
        bool dri_active = QFileInfo::exists(QStringLiteral("/dev/dri"));
        beginResetModel();
        qDeleteAll(m_sessions);
        m_sessions.clear();
        m_displayNames.clear();
        if (dri_active)
            populate(Session::WaylandSession, mainConfig.Wayland.SessionDir.get());
        populate(Session::X11Session, mainConfig.X11.SessionDir.get());
        m_currentIndex = 0;
        endResetModel();
        Q_EMIT currentIndexChanged(m_currentIndex);
    });
    watcher->addPaths(mainConfig.Wayland.SessionDir.get());
    watcher->addPaths(mainConfig.X11.SessionDir.get());
}

SessionModel::~SessionModel()
{
    qDeleteAll(m_sessions);
    m_sessions.clear();
}

QHash<int, QByteArray> SessionModel::roleNames() const
{
    // set role names
    QHash<int, QByteArray> roleNames;
    roleNames[DirectoryRole] = QByteArrayLiteral("directory");
    roleNames[FileRole] = QByteArrayLiteral("file");
    roleNames[TypeRole] = QByteArrayLiteral("type");
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
    Session *session = m_sessions[index.row()];
    Q_ASSERT(session);

    // return correct value
    switch (role) {
    case DirectoryRole:
        return session->directory().absolutePath();
    case FileRole:
        return session->fileName();
    case TypeRole:
        return session->type();
    case NameRole:
        if (m_displayNames.count(session->displayName()) > 1
            && session->type() == Session::WaylandSession)
            return tr("%1 (Wayland)").arg(session->displayName());
        return session->displayName();
    case ExecRole:
        return session->exec();
    case CommentRole:
        return session->comment();
    default:
        break;
    }

    // return empty value
    return QVariant();
}

void SessionModel::populate(DDM::Session::Type type, const QStringList &dirPaths)
{
    // read session files
    QStringList sessions;
    for (const auto &path : dirPaths) {
        QDir dir = path;
        dir.setNameFilters(QStringList() << QStringLiteral("*.desktop"));
        dir.setFilter(QDir::Files);
        sessions += dir.entryList();
    }
    // read session
    sessions.removeDuplicates();
    for (auto &session : std::as_const(sessions)) {
        Session *si = new Session(type, session);
        bool execAllowed = true;
        QFileInfo fi(si->tryExec());
        if (fi.isAbsolute()) {
            if (!fi.exists() || !fi.isExecutable())
                execAllowed = false;
        } else {
            execAllowed = false;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            QString envPath = env.value(QStringLiteral("PATH"));
            const QStringList pathList = envPath.split(QLatin1Char(':'));
            for (const QString &path : pathList) {
                QDir pathDir(path);
                fi.setFile(pathDir, si->tryExec());
                if (fi.exists() && fi.isExecutable()) {
                    execAllowed = true;
                    break;
                }
            }
        }
        // add to sessions list
        if (execAllowed) {
            m_displayNames.append(si->displayName());
            if (si->displayName() == QStringLiteral("Treeland"))
                m_sessions.prepend(si);
            else
                m_sessions.push_back(si);
        } else {
            delete si;
        }
    }
    // find out index of the last session
    if (mainConfig.Users.RememberLastSession.get())
        for (int i = 0; i < m_sessions.size(); ++i) {
            if (m_sessions.at(i)->fileName() == stateConfig.Last.Session.get()) {
                m_currentIndex = i;
                break;
            }
        }
}
