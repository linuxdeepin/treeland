/**************************************************************************
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

#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>

class QFileSystemWatcher;

class SessionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY(SessionModel)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    QML_NAMED_ELEMENT(SessionModel)
    QML_SINGLETON

public:
    enum SessionRole
    {
        DirectoryRole = Qt::UserRole + 1,
        FileRole,
        NameRole,
        ExecRole,
        CommentRole
    };
    Q_ENUM(SessionRole)

    SessionModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;

    inline int currentIndex() const { return m_currentIndex; };
    void setCurrentIndex(int index);

    // Directories the sessions are installed in, sent by DDM when the greeter
    // connects. Sessions are listed for them, nothing is listed before.
    void setSessionDirs(const QStringList &waylandSessionDirs, const QStringList &x11SessionDirs);

    // Session of the last successful login, sent by DDM. Applied whenever the
    // model is populated or the value arrives after the dirs.
    void setLastSession(const QString &filePath);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

Q_SIGNALS:
    void currentIndexChanged(int index);

private:
    // A session desktop file, parsed for the fields the greeter shows and
    // reports back to DDM.
    struct SessionEntry
    {
        bool wayland; // listed from the Wayland session directories
        QString filePath; // absolute path of the desktop file
        QString displayName;
        QString exec;
        QString comment;
        QString tryExec;
    };

    void populate(bool wayland, const QStringList &dirPaths);
    void reload();
    int indexOfSession(const QString &filePath) const;

    int m_currentIndex{ 0 };
    QString m_lastSession{};
    QStringList m_displayNames{};
    QList<SessionEntry> m_sessions{};
    QStringList m_waylandSessionDirs{};
    QStringList m_x11SessionDirs{};
    QFileSystemWatcher *m_watcher{ nullptr };
};

QML_DECLARE_TYPE(SessionModel)
