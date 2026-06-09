// SPDX-License-Identifier: GPL-3.0-only
/***************************************************************************
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
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

#include <QFileInfo>

SessionModel::SessionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QHash<int, QByteArray> SessionModel::roleNames() const
{
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
    if (m_currentIndex == index)
        return;

    m_currentIndex = index;
    Q_EMIT currentIndexChanged(index);
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sessions.length());
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_sessions.count())
        return QVariant();

    const auto &session = m_sessions.at(index.row());

    switch (role) {
    case DirectoryRole:
        return QFileInfo(session.fileName()).absolutePath();
    case FileRole:
        return session.fileName();
    case TypeRole:
        return session.type();
    case NameRole:
        if (m_displayNames.count(session.displayName()) > 1 && session.type() == WaylandSession)
            return tr("%1 (Wayland)").arg(session.displayName());
        return session.displayName();
    case ExecRole:
        return session.exec();
    case CommentRole:
        return session.comment();
    default:
        break;
    }

    return QVariant();
}

void SessionModel::setSessions(const QList<SessionEntry> &sessions)
{
    beginResetModel();
    m_sessions = sessions;
    m_displayNames.clear();
    for (const auto &session : std::as_const(m_sessions))
        m_displayNames.append(session.displayName());
    updateCurrentIndex();
    endResetModel();
    Q_EMIT currentIndexChanged(m_currentIndex);
}

void SessionModel::setLastSession(const QString &lastSession)
{
    if (m_lastSession == lastSession)
        return;

    m_lastSession = lastSession;
    updateCurrentIndex();
}

void SessionModel::setRememberLastSession(bool rememberLastSession)
{
    if (m_rememberLastSession == rememberLastSession)
        return;

    m_rememberLastSession = rememberLastSession;
    updateCurrentIndex();
}

void SessionModel::updateCurrentIndex()
{
    int index = 0;
    if (m_rememberLastSession && !m_lastSession.isEmpty()) {
        for (int i = 0; i < m_sessions.size(); ++i) {
            if (m_sessions.at(i).fileName() == m_lastSession) {
                index = i;
                break;
            }
        }
    }

    setCurrentIndex(index);
}
