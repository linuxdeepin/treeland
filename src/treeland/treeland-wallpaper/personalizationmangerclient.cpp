// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmangerclient.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QtWaylandClient/QWaylandClientExtension>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
    , m_metaData(new WallpaperMetaData())
{
    connect(this, &PersonalizationManager::activeChanged, this, &PersonalizationManager::onActiveChanged);
    m_cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/wallpaper/";
}

PersonalizationManager::~PersonalizationManager()
{
    if (m_wallpaperContext) {
        m_wallpaperContext->deleteLater();
    }

    if (m_modes.size() > 0) {
        qDeleteAll(m_modes);
    }

    if (m_metaData) {
        delete m_metaData;
    }
}

void PersonalizationManager::onActiveChanged()
{
    if (!isActive())
        return;

    if (!m_wallpaperContext) {
        m_wallpaperContext = new PersonalizationWallpaper(get_wallpaper_context());
        connect(m_wallpaperContext, &PersonalizationWallpaper::wallpaperChanged,
                this, &PersonalizationManager::onWallpaperChanged);
    }

    m_wallpaperContext->get_wallpapers();
}

void PersonalizationManager::addWallpaper(const QString &path)
{
    if (!m_wallpaperContext)
        return;

    QString local_path = QUrl(path).toLocalFile();

    QFileInfo file_info(local_path);
    QString dest = m_cacheDirectory + file_info.fileName();

    QDir dir(m_cacheDirectory);
    if (!dir.exists()) {
        dir.mkpath(m_cacheDirectory);
    }

    if (QFile::copy(local_path, dest)) {
        QFile file(dest);
        if (file.open(QIODevice::ReadOnly)) {
            m_modes["Local"]->append(dest);
            m_metaData->imagePath = dest;
            m_metaData->currentIndex = m_modes["Local"]->currentIndex();

            m_wallpaperContext->set_wallpaper(file.handle(), converToJson(m_metaData));
            Q_EMIT wallpaperChanged(dest);
        }
    }
}

void PersonalizationManager::removeWallpaper(const QString &path, const QString &group, int index)
{
    if (m_modes[group]->currentIndex() == index)
        return;

    QString local_path = QUrl(path).toLocalFile();
    QFile::remove(local_path);

    m_modes[group]->remove(index);
}

void PersonalizationManager::setWallpaper(const QString &path, const QString &group, int index)
{
    if (!m_wallpaperContext)
        return;

    QString dest = QUrl(path).toLocalFile();
    if (dest.isEmpty())
        return;

    QFile file(dest);
    if (file.open(QIODevice::ReadOnly)) {
        m_modes[group]->setCurrentIndex(index);

        m_metaData->imagePath = dest;
        m_metaData->currentIndex = index;
        setCurrentGroup(group);

        m_wallpaperContext->set_wallpaper(file.handle(), converToJson(m_metaData));
        Q_EMIT wallpaperChanged(dest);
    }
}

QString PersonalizationManager::currentGroup()
{
    return m_metaData->group;
}

void PersonalizationManager::setCurrentGroup(const QString& group)
{
    m_metaData->group = group;
    Q_EMIT currentGroupChanged(group);
}

QString PersonalizationManager::cacheDirectory()
{
    return m_cacheDirectory;
}

WallpaperCardModel* PersonalizationManager::wallpaperModel(const QString &group, const QString &dir)
{
    if (m_modes.contains(group))
        return m_modes[group];

    auto model = new WallpaperCardModel(this);
    model->setDirectory(dir);;
    m_modes[group] = model;
    return m_modes[group];
}

QString PersonalizationManager::converToJson(WallpaperMetaData *data)
{
    QJsonObject json;
    json.insert("Group", data->group);
    json.insert("ImagePath", data->imagePath);
    json.insert("Output", data->output);
    json.insert("CurrentIndex", data->currentIndex);

    QJsonDocument json_doc(json);
    return json_doc.toJson(QJsonDocument::Compact);
}

void PersonalizationManager::onWallpaperChanged(const QString& meta)
{
    QJsonDocument json_doc = QJsonDocument::fromJson(meta.toLocal8Bit());

    if (!json_doc.isNull()) {
        QJsonObject json_obj = json_doc.object();

        m_metaData->currentIndex = json_obj["CurrentIndex"].toInt();
        m_metaData->group = json_obj["Group"].toString();
        m_metaData->imagePath = json_obj["ImagePath"].toString();
        m_metaData->output = json_obj["Output"].toString();
    }

    auto model = m_modes[m_metaData->group];
    if (model) {
        model->setCurrentIndex(m_metaData->currentIndex);
    }
    Q_EMIT wallpaperChanged(m_metaData->imagePath);
}

PersonalizationWindow::PersonalizationWindow(struct ::personalization_window_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWindow>(1)
    , QtWayland::personalization_window_context_v1(object)
{

}

PersonalizationWallpaper::PersonalizationWallpaper(struct ::personalization_wallpaper_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWallpaper>(1)
    , QtWayland::personalization_wallpaper_context_v1(object)
{

}

void PersonalizationWallpaper::personalization_wallpaper_context_v1_wallpapers(const QString &metadata)
{
    Q_EMIT wallpaperChanged(metadata);
}
