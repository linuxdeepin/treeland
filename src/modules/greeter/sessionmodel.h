// SPDX-License-Identifier: GPL-3.0-only
/**************************************************************************
 * Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
 * Copyright (c) 2015-2016 Pier Luigi Fiorini <pierluigi@gmail.com>
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
 **************************************************************************/

#pragma once

#include <rep_ddmremote_replica.h>

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>

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
        TypeRole,
        NameRole,
        ExecRole,
        CommentRole
    };
    Q_ENUM(SessionRole)

    enum SessionType
    {
        UnknownSession = 0,
        X11Session = 1,
        WaylandSession = 2,
    };
    Q_ENUM(SessionType)

    explicit SessionModel(QObject *parent = nullptr);

    QHash<int, QByteArray> roleNames() const override;

    int currentIndex() const
    {
        return m_currentIndex;
    }

    void setCurrentIndex(int index);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setSessions(const QList<SessionEntry> &sessions);
    void setLastSession(const QString &lastSession);
    void setRememberLastSession(bool rememberLastSession);

Q_SIGNALS:
    void currentIndexChanged(int index);

private:
    void updateCurrentIndex();

    int m_currentIndex{ 0 };
    bool m_rememberLastSession{ false };
    QString m_lastSession;
    QStringList m_displayNames;
    QList<SessionEntry> m_sessions;
};

QML_DECLARE_TYPE(SessionModel)
