// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercardmodel.h"
#include "Constants.h"
#include "Configuration.h"
#include <QDir>
#include <QFile>
#include <QList>

struct WallpaperCardModelPrivate {
    int currentIndex {0};
    QString directory;
    QStringList wallpapers;
};

WallpaperCardModel::WallpaperCardModel(QObject *parent)
    : QAbstractListModel(parent)
    , d(new WallpaperCardModelPrivate())
{
}

WallpaperCardModel::~WallpaperCardModel() {
    delete d;
}

QHash<int, QByteArray> WallpaperCardModel::roleNames() const {
    // set role names
    static auto roleNames = [](){
        QHash<int, QByteArray> names;
        names[ImageSourceRole] = QByteArrayLiteral("imageSource");
        return names;
    }();

    return roleNames;
}

void WallpaperCardModel::setCurrentIndex(int index)
{
    d->currentIndex = index;
    Q_EMIT currentIndexChanged(d->currentIndex);
}

int WallpaperCardModel::currentIndex() const {
    return d->currentIndex;
}

int WallpaperCardModel::rowCount(const QModelIndex &parent) const {
    return static_cast<int>(d->wallpapers.length());
}

QVariant WallpaperCardModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() > d->wallpapers.count())
        return {};

    // return correct value
    switch(role) {
    case ImageSourceRole:
        return  d->wallpapers.at(index.row());
    default:
        return {};
    }

    Q_UNREACHABLE();
}

QString WallpaperCardModel::directory()
{
    return d->directory;
}

void WallpaperCardModel::setDirectory(const QString& directory)
{
    QDir dir(directory);
    d->directory = directory;

    QStringList entries = dir.entryList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"});
    for (QStringList::iterator it = entries.begin(); it != entries.end(); ++it) {
        d->wallpapers.push_back("file://" + dir.absolutePath() + "/" + *it);
    }
}

void WallpaperCardModel::append(const QString& path)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    d->wallpapers.push_back("file://" + path);
    setCurrentIndex(rowCount());
    endInsertRows();
}
