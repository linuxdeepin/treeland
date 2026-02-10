// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapermanager.h"
#include "workspace.h"
#include "helper.h"
#include "treelanduserconfig.hpp"
#include "common/treelandlogging.h"
#include "modules/wallpaper/wallpapershellinterfacev1.h"
#include "shellhandler.h"

#include <QMimeDatabase>
#include <QMimeType>

#define DEFAULT_WALLPAPER "/usr/share/wallpapers/deepin/deepin-default.jpg"

enum class WallpaperType {
    Image,
    Video,
    Unknown
};

static WallpaperType detectWallpaperType(const QString &path)
{
    if (path.isEmpty())
        return WallpaperType::Unknown;

    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    if (mime.isValid() && mime.name().startsWith("image/")) {
        return WallpaperType::Image;
    }

    if (mime.isValid() && mime.name().startsWith("video/")) {
        return WallpaperType::Video;
    }

    return WallpaperType::Unknown;
}

WallpaperManager::WallpaperManager(QObject *parent)
    : QObject(parent)
{
}

WallpaperManager::~WallpaperManager()
{

}

QString WallpaperManager::getOutputId(wlr_output *output)
{
    QString model = output->model;
    QString outputId = model.append(output->serial);

    return outputId;
}

QString WallpaperManager::getOutputId(Output *output)
{
    return getOutputId(output->output()->nativeHandle());
}

WallpaperOutputConfig WallpaperManager::getOutputConfig(Output *output)
{
    return getOutputConfig(getOutputId(output));
}

WallpaperOutputConfig WallpaperManager::getOutputConfig(wlr_output *output)
{
    return getOutputConfig(getOutputId(output));
}

WallpaperOutputConfig WallpaperManager::getOutputConfig(const QString &id)
{
    for (WallpaperOutputConfig output : m_wallpaperConfig) {
        if (output.outputName == id) {
            return output;
        }
    }

    return WallpaperOutputConfig{};
}

void WallpaperManager::updateWallpaperConfig()
{
    m_wallpaperConfig.clear();
    if (!Helper::instance()->m_config->wallpaperConfig().isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(Helper::instance()->m_config->wallpaperConfig().toUtf8());
        if (doc.isArray()) {
            QJsonArray jsonArray = doc.array();
            for (const QJsonValue& value : jsonArray) {
                QJsonObject obj = value.toObject();
                m_wallpaperConfig.append(WallpaperOutputConfig::fromJson(obj));
            }
        } else {
            qCCritical(treelandWallpaper) << "wallpapers config value error: Expected a JSON array, but got something else.";
        }
    } else {
        defaultWallpaperConfig();
    }
    m_wallpaperConfigUpdated = true;
}

void WallpaperManager::defaultWallpaperConfig()
{
    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);
    for (Output *output : Helper::instance()->m_outputList) {
        WallpaperOutputConfig outputConfig;
        outputConfig.lockscreenWallpaper = Helper::instance()->m_config->defaultBackground();
        outputConfig.outputName = WallpaperManager::getOutputId(output);
        outputConfig.enable = output->output()->nativeHandle()->enabled;
        WallpaperType type = detectWallpaperType(outputConfig.lockscreenWallpaper);
        if (type == WallpaperType::Unknown) {
            outputConfig.lockscreenWallpaper = DEFAULT_WALLPAPER;
            outputConfig.lockScreenWallpapertype = TreelandWallpaperInterfaceV1::Image;
        } else {
            outputConfig.lockScreenWallpapertype = static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(type);
        }

        for (int i = 0; i < workspace->count(); i++) {
            WallpaperWorkspaceConfig workspaceConfig;
            workspaceConfig.desktopWallpaper = Helper::instance()->m_config->defaultBackground();
            workspaceConfig.workspaceId = i;
            workspaceConfig.enable = true;
            WallpaperType type = detectWallpaperType(workspaceConfig.desktopWallpaper);
            if (type == WallpaperType::Unknown) {
                workspaceConfig.desktopWallpaper = DEFAULT_WALLPAPER;
                workspaceConfig.desktopWallpapertype = TreelandWallpaperInterfaceV1::Image;
            } else {
                workspaceConfig.desktopWallpapertype = static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(type);
            }
            outputConfig.workspaces.append(workspaceConfig);
        }
        m_wallpaperConfig.append(outputConfig);
    }

    QString json = wallpaperConfigToJsonString();
    Helper::instance()->m_config->setWallpaperConfig(json);
}

void WallpaperManager::ensureWallpaperConfigForOutput(Output *output)
{
    if (!m_wallpaperConfigUpdated) {
        return;
    }

    bool update = false;
    if (!configContainsOutput(output)) {
        WallpaperOutputConfig refConfig = m_wallpaperConfig.last();
        WallpaperWorkspaceConfig refWorkspaceConfig = refConfig.workspaces.first();
        Workspace *workspace = Helper::instance()->workspace();
        Q_ASSERT(workspace);

        WallpaperOutputConfig outputConfig;
        outputConfig.lockscreenWallpaper = refConfig.lockscreenWallpaper;
        outputConfig.lockScreenWallpapertype = refConfig.lockScreenWallpapertype;
        outputConfig.outputName = getOutputId(output);
        outputConfig.enable = output->output()->nativeHandle()->enabled;
        for (int i = 0; i < workspace->count(); i++) {
            WallpaperWorkspaceConfig workspaceConfig;
            workspaceConfig.desktopWallpaper = refWorkspaceConfig.desktopWallpaper;
            workspaceConfig.desktopWallpapertype = refWorkspaceConfig.desktopWallpapertype;
            workspaceConfig.workspaceId = i;
            workspaceConfig.enable = true;
            outputConfig.workspaces.append(workspaceConfig);
        }
        m_wallpaperConfig.append(outputConfig);
        update = true;
    } else {
        return;
    }

    if (update) {
        QString json = wallpaperConfigToJsonString();
        Helper::instance()->m_config->setWallpaperConfig(json);
        Q_EMIT updateWallpaper();
    }
}

bool WallpaperManager::configContainsOutput(Output *output)
{
    if (m_wallpaperConfig.isEmpty()) {
        return false;
    }

    QString outputId = getOutputId(output);
    for (const WallpaperOutputConfig& output : m_wallpaperConfig) {
        if (output.outputName == outputId) {
            return true;
        }
    }

    return false;
}

QString WallpaperManager::wallpaperConfigToJsonString()
{
    QJsonArray array;
    for (const auto &cfg : m_wallpaperConfig) {
        array.append(cfg.toJson());
    }

    QJsonDocument doc(array);
    return QString::fromUtf8(
        doc.toJson(QJsonDocument::Indented));
}

void WallpaperManager::setOutputWallpaper(wlr_output *output, [[maybe_unused]] int workspaceIndex, const QString &fileSource, TreelandWallpaperInterfaceV1::WallpaperRoles roles, TreelandWallpaperInterfaceV1::WallpaperType type)
{
    bool update = false;
    for (WallpaperOutputConfig &outputConfig : m_wallpaperConfig) {
        if (outputConfig.outputName == getOutputId(output)) {
            if (roles.testFlag(TreelandWallpaperInterfaceV1::Lockscreen)) {
                if (outputConfig.lockscreenWallpaper != fileSource) {
                    update = true;
                    outputConfig.lockscreenWallpaper = fileSource;
                    outputConfig.lockScreenWallpapertype = type;
                }
            }

            if (roles.testFlag(TreelandWallpaperInterfaceV1::Desktop)) {
                for (WallpaperWorkspaceConfig &workspaceConfig : outputConfig.workspaces) {
                    if (workspaceConfig.desktopWallpaper != fileSource) {
                        update = true;
                        workspaceConfig.desktopWallpaper = fileSource;
                        workspaceConfig.desktopWallpapertype = type;
                    }
                }
            }

            break;
        }
    }

    if (update) {
        Helper::instance()->m_config->setWallpaperConfig(wallpaperConfigToJsonString());
    }
}

QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> WallpaperManager::globalValidWallpaper(wlr_output *exclusiveOutput, int exclusiveworkspaceId)
{
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> wallpapers;
    for (const WallpaperOutputConfig& output : m_wallpaperConfig) {
        if (!wallpapers.contains(output.lockscreenWallpaper)) {
            if (!(exclusiveOutput &&
                  output.outputName == getOutputId(exclusiveOutput)) && output.enable) {
                wallpapers.insert(output.lockscreenWallpaper, output.lockScreenWallpapertype);
            }
        }
        for (const WallpaperWorkspaceConfig& workspace : output.workspaces) {
            if (exclusiveOutput &&
                output.outputName == getOutputId(exclusiveOutput) &&
                workspace.workspaceId == exclusiveworkspaceId) {
                continue;
            }

            if (!wallpapers.contains(workspace.desktopWallpaper) && workspace.enable) {
                wallpapers.insert(workspace.desktopWallpaper, workspace.desktopWallpapertype);
            }
        }
    }

    return wallpapers;
}

void WallpaperManager::syncAddWorkspace()
{
    if (!m_wallpaperConfigUpdated) {
        return;
    }

    bool update = false;
    for (WallpaperOutputConfig &outputConfig : m_wallpaperConfig) {
        WallpaperWorkspaceConfig refConfig = outputConfig.workspaces[0];
        Workspace *workspace = Helper::instance()->workspace();
        Q_ASSERT(workspace);
        for (int i = 0; i < workspace->count(); i++) {
            if (i >= outputConfig.workspaces.count()) {
                WallpaperWorkspaceConfig workspaceConfig;
                workspaceConfig.desktopWallpaper = refConfig.desktopWallpaper;
                workspaceConfig.desktopWallpapertype = refConfig.desktopWallpapertype;
                workspaceConfig.workspaceId = i;
                workspaceConfig.enable = true;
                outputConfig.workspaces.append(workspaceConfig);
                update = true;
            }
        }
    }

    if (update) {
        Helper::instance()->m_config->setWallpaperConfig(wallpaperConfigToJsonString());
    }
}

void WallpaperManager::removeOutputWallpaper(wlr_output *output)
{
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(output, -1);
    for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
        if (m_wallpaperConfig[i].outputName == getOutputId(output)) {
            WallpaperOutputConfig &outputConfig = m_wallpaperConfig[i];
            outputConfig.enable = false;
            if (!globalWallpapers.contains(outputConfig.lockscreenWallpaper)) {
                Helper::instance()->m_wallpaperNotifierInterfaceV1->sendRemove(outputConfig.lockscreenWallpaper);
            }
            for (WallpaperWorkspaceConfig &workspace : outputConfig.workspaces) {
                workspace.enable = false;
                if (!globalWallpapers.contains(workspace.desktopWallpaper)) {
                    Helper::instance()->m_wallpaperNotifierInterfaceV1->sendRemove(workspace.desktopWallpaper);
                }
            }
            break;
        }
    }
}

QString WallpaperManager::currentWorkspaceWallpaper(WOutput *output)
{
    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);
    for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
        if (m_wallpaperConfig[i].outputName == getOutputId(output->nativeHandle())) {
            return m_wallpaperConfig[i].workspaces[workspace->currentIndex()].desktopWallpaper;
        }
    }

    return QStringLiteral("unknown");
}

TreelandWallpaperInterfaceV1::WallpaperType WallpaperManager::getWallpaperType(const QString &wallpaper)
{
    for (WallpaperOutputConfig outputConfig : std::as_const(m_wallpaperConfig)) {
        if (outputConfig.lockscreenWallpaper == wallpaper) {
            return outputConfig.lockScreenWallpapertype;
        }

        for (WallpaperWorkspaceConfig workspaceConfig : std::as_const(outputConfig.workspaces)) {
            if (workspaceConfig.desktopWallpaper == wallpaper) {
                return workspaceConfig.desktopWallpapertype;
            }
        }
    }

    qCCritical(treelandWallpaper) << "Failed find wallpaper type for" << wallpaper;
    return TreelandWallpaperInterfaceV1::Image;
}

void WallpaperManager::onWallpaperAdded(TreelandWallpaperInterfaceV1 *interface)
{
    connect(interface,
        &TreelandWallpaperInterfaceV1::binded,
        this, [this, interface]{
            Workspace *workspace = Helper::instance()->workspace();
            Q_ASSERT(workspace);
            for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
                if (m_wallpaperConfig[i].outputName == getOutputId(interface->wOutput()->nativeHandle())) {
                    WallpaperOutputConfig outputConfig = m_wallpaperConfig[i];
                    interface->sendChanged(TreelandWallpaperInterfaceV1::Lockscreen, outputConfig.lockScreenWallpapertype, outputConfig.lockscreenWallpaper);
                    for (WallpaperWorkspaceConfig workspaceConfig : outputConfig.workspaces) {
                        if (workspaceConfig.workspaceId == workspace->currentIndex()) {
                            interface->sendChanged(TreelandWallpaperInterfaceV1::Desktop, workspaceConfig.desktopWallpapertype, workspaceConfig.desktopWallpaper);
                        }
                    }
                    break;
                }
            }
        }, Qt::SingleShotConnection);
    connect(interface,
            &TreelandWallpaperInterfaceV1::imageSourceChanged,
            this,
            &WallpaperManager::onImageChanged);
    connect(interface,
            &TreelandWallpaperInterfaceV1::videoSourceChanged,
            this,
            &WallpaperManager::onVideoChanged);
}

void WallpaperManager::onImageChanged(int workspaceIndex, const QString &fileSource, TreelandWallpaperInterfaceV1::WallpaperRoles roles)
{
    TreelandWallpaperInterfaceV1 *interface =
        static_cast<TreelandWallpaperInterfaceV1 *>(sender());
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    setOutputWallpaper(interface->wOutput()->nativeHandle(),
                       workspaceIndex,
                       fileSource,
                       roles,
                       TreelandWallpaperInterfaceV1::Image);
    if (globalWallpapers.contains(fileSource)) {
        interface->sendError(fileSource, TreelandWallpaperInterfaceV1::AlreadyUsed);
        Q_EMIT updateWallpaper();
    } else {
        Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(TreelandWallpaperInterfaceV1::Image, fileSource);
    }
}

void WallpaperManager::onVideoChanged(int workspaceIndex, const QString &fileSource, TreelandWallpaperInterfaceV1::WallpaperRoles roles)
{
    TreelandWallpaperInterfaceV1 *interface =
        static_cast<TreelandWallpaperInterfaceV1 *>(sender());
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    setOutputWallpaper(interface->wOutput()->nativeHandle(),
                       workspaceIndex,
                       fileSource,
                       roles,
                       TreelandWallpaperInterfaceV1::Video);
    if (globalWallpapers.contains(fileSource)) {
        interface->sendError(fileSource, TreelandWallpaperInterfaceV1::AlreadyUsed);
        Q_EMIT updateWallpaper();
    } else {
        Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(TreelandWallpaperInterfaceV1::Video, fileSource);
    }
}

void WallpaperManager::onWallpaperNotifierbinded()
{
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    QMapIterator<QString, TreelandWallpaperInterfaceV1::WallpaperType> i(globalWallpapers);
    while (i.hasNext()) {
        i.next();
        Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(i.value()), i.key());
    }
}

void WallpaperManager::handleWallpaperSurfaceAdded([[maybe_unused]] TreelandWallpaperSurfaceInterfaceV1 *interface)
{
    QList<QString> wallpapers = Helper::instance()->shellHandler()->wallpaperShell()->producedWallpapers();
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    foreach (auto wallpaper, std::as_const(wallpapers)) {
        if (!globalWallpapers.contains(wallpaper)) {
            Helper::instance()->m_wallpaperNotifierInterfaceV1->sendRemove(wallpaper);
        }
    }
}

