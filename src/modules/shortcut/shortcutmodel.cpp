// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmodel.h"

ShortcutModel::ShortcutModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVariant ShortcutModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || (!m_user.isEmpty() && index.row() > m_actions[m_user].size())) {
        return {};
    }

    switch (role) {
    case SequenceRole:
        return m_actions[m_user][index.row()]->shortcut();
    default:
        return {};
    }

    Q_UNREACHABLE();
}

int ShortcutModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    if (m_user.isEmpty()) {
        return 0;
    }

    return m_actions[m_user].size();
}

void ShortcutModel::setUser(const QString &user)
{
    if (user == m_user) {
        return;
    }

    m_user = user;

    Q_EMIT userChanged();
    Q_EMIT layoutChanged();
}

QHash<int, QByteArray> ShortcutModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[SequenceRole] = QByteArrayLiteral("shortcut");
    return names;
}

bool ShortcutModel::addAction(const QString &user, QAction *action)
{
    if (!m_actions.count(user)) {
        m_actions[user] = {};
    }

    auto find = std::ranges::find_if(m_actions[user], [action](QAction *a) {
        return a->shortcut() == action->shortcut();
    });

    if (find == m_actions[user].end()) {
        m_actions[user].push_back(action);
        Q_EMIT layoutChanged();
    }

    return find == m_actions[user].end();
}

void ShortcutModel::removeAction(const QString &user, QAction *action)
{
    if (!m_actions.count(user)) {
        return;
    }

    std::erase_if(m_actions[user], [action](QAction *a) {
        return a == action;
    });

    Q_EMIT layoutChanged();
}

void ShortcutModel::trigger(int index)
{
    Q_ASSERT(!m_user.isEmpty());
    Q_ASSERT(index < m_actions[m_user].size());

    m_actions[m_user][index]->trigger();
}

void ShortcutModel::handleMetaKey()
{
    if (m_user.isEmpty()) {
        return;
    }

    for (auto *action : m_actions[m_user]) {
        if (action->shortcut().toString() == "Meta") {
            action->trigger();
            break;
        }
    }
}
