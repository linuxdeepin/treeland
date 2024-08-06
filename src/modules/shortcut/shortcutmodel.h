// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QAbstractListModel>
#include <QAction>
#include <QQmlEngine>

class ShortcutModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged FINAL)

public:
    explicit ShortcutModel(QObject *parent = nullptr);

    enum UserRoles {
        NameRole = Qt::UserRole + 1,
        SequenceRole,
    };
    Q_ENUM(UserRoles)

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QString user() const { return m_user; }

    void setUser(const QString &user);

    Q_INVOKABLE void trigger(int index);
    Q_INVOKABLE void handleMetaKey();

Q_SIGNALS:
    void userChanged();

private:
    friend class ShortcutV1;
    bool addAction(const QString &user, QAction *action);
    void removeAction(const QString &user, QAction *action);

private:
    QString m_user;
    QMap<QString, std::vector<QAction *>> m_actions;
};
