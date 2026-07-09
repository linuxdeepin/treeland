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
    const QString model = QString::fromUtf8(output->model);
    const QString serial = QString::fromUtf8(output->serial);
    if (!model.isEmpty() && !serial.isEmpty()) {
        return model + serial;
    }

    return QString::fromUtf8(output->name);
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
    for (WallpaperOutputConfig output : std::as_const(m_wallpaperConfig)) {
        if (output.outputName == id) {
            return output;
        }
    }

    return WallpaperOutputConfig{};
}

void WallpaperManager::updateWallpaperConfig()
{
    m_wallpaperConfig.clear();
    if (!Helper::instance()->config()->wallpaperConfig().isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(Helper::instance()->config()->wallpaperConfig().toUtf8());
        if (doc.isArray()) {
            QJsonArray jsonArray = doc.array();
            for (const QJsonValue& value : jsonArray) {
                QJsonObject obj = value.toObject();
                m_wallpaperConfig.append(WallpaperOutputConfig::fromJson(obj));
            }
        } else {
            qCCritical(lcTlWallpaper) << "wallpapers config value error: Expected a JSON array, but got something else.";
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
    for (Output *output : std::as_const(Helper::instance()->m_outputList)) {
        WallpaperOutputConfig outputConfig;
        outputConfig.lockscreenWallpaper = Helper::instance()->config()->defaultBackground();
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
            workspaceConfig.desktopWallpaper = Helper::instance()->config()->defaultBackground();
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
    Helper::instance()->config()->setWallpaperConfig(json);
}

void WallpaperManager::ensureWallpaperConfigForOutput(Output *output)
{
    if (!m_wallpaperConfigUpdated) {
        return;
    }

    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> beforeWallpapers =
        globalValidWallpaper(nullptr, -1);

    bool update = false;
    bool isNewOutput = false;
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
        isNewOutput = true;
    } else {
        QString outputId = getOutputId(output);
        Workspace *workspace = Helper::instance()->workspace();
        Q_ASSERT(workspace);

        for (WallpaperOutputConfig &outputConfig : m_wallpaperConfig) {
            if (outputConfig.outputName == outputId) {
                outputConfig.enable = true;
                for (WallpaperWorkspaceConfig &wsConfig : outputConfig.workspaces) {
                    wsConfig.enable = true;
                }
                const WallpaperWorkspaceConfig refConfig = outputConfig.workspaces.first();
                for (int i = 0; i < workspace->count(); i++) {
                    const int workspaceId = workspace->modelAt(i)->id();
                    if (!outputConfig.containsWorkspace(workspaceId)) {
                        WallpaperWorkspaceConfig wsConfig;
                        wsConfig.desktopWallpaper = refConfig.desktopWallpaper;
                        wsConfig.desktopWallpapertype = refConfig.desktopWallpapertype;
                        wsConfig.workspaceId = workspaceId;
                        wsConfig.enable = true;
                        outputConfig.workspaces.append(wsConfig);
                    }
                }
                update = true;
                break;
            }
        }
    }

    if (update) {
        QString json = wallpaperConfigToJsonString();
        Helper::instance()->config()->setWallpaperConfig(json);

        if (isNewOutput) {
            Workspace *workspace = Helper::instance()->workspace();
            Q_ASSERT(workspace);

            sendMissingWallpapersForNewOutput(getOutputConfig(output), workspace, beforeWallpapers);
        } else {
            QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> afterWallpapers =
                globalValidWallpaper(nullptr, -1);
            for (auto it = afterWallpapers.constBegin(); it != afterWallpapers.constEnd(); ++it) {
                if (!beforeWallpapers.contains(it.key())) {
                    Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(it.value(), it.key());
                }
            }
        }

        Q_EMIT updateWallpaper();
    }
}

void WallpaperManager::sendMissingWallpapersForNewOutput(
    const WallpaperOutputConfig &outputConfig,
    Workspace *workspace,
    const QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> &beforeWallpapers)
{
    QString currentDesktopWallpaper;
    const int currentIndex = workspace->currentIndex();
    if (currentIndex >= 0 && currentIndex < outputConfig.workspaces.size()) {
        const WallpaperWorkspaceConfig &workspaceConfig = outputConfig.workspaces[currentIndex];
        currentDesktopWallpaper = workspaceConfig.desktopWallpaper;
        if (!beforeWallpapers.contains(workspaceConfig.desktopWallpaper)) {
            Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(workspaceConfig.desktopWallpapertype,
                                                                         workspaceConfig.desktopWallpaper);
        }
    }

    if (!beforeWallpapers.contains(outputConfig.lockscreenWallpaper)
        && currentDesktopWallpaper != outputConfig.lockscreenWallpaper) {
        Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAdd(outputConfig.lockScreenWallpapertype,
                                                                     outputConfig.lockscreenWallpaper);
    }
}

bool WallpaperManager::configContainsOutput(Output *output)
{
    if (m_wallpaperConfig.isEmpty()) {
        return false;
    }

    QString outputId = getOutputId(output);
    for (const WallpaperOutputConfig& output : std::as_const(m_wallpaperConfig)) {
        if (output.outputName == outputId) {
            return true;
        }
    }

    return false;
}

QString WallpaperManager::wallpaperConfigToJsonString()
{
    QJsonArray array;
    for (const auto &cfg : std::as_const(m_wallpaperConfig)) {
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
        Helper::instance()->config()->setWallpaperConfig(wallpaperConfigToJsonString());
    }
}

QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> WallpaperManager::globalValidWallpaper(wlr_output *exclusiveOutput, int exclusiveworkspaceId)
{
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> wallpapers;
    for (const WallpaperOutputConfig& output : std::as_const(m_wallpaperConfig)) {
        bool outputConnected = false;
        for (Output *connectedOutput : std::as_const(Helper::instance()->m_outputList)) {
            if (output.outputName == getOutputId(connectedOutput)) {
                outputConnected = true;
                break;
            }
        }
        if (!outputConnected) {
            continue;
        }

        if (!wallpapers.contains(output.lockscreenWallpaper)) {
            if (!(exclusiveOutput &&
                  output.outputName == getOutputId(exclusiveOutput))) {
                wallpapers.insert(output.lockscreenWallpaper, output.lockScreenWallpapertype);
            }
        }
        for (const WallpaperWorkspaceConfig& workspace : std::as_const(output.workspaces)) {
            if (exclusiveOutput &&
                output.outputName == getOutputId(exclusiveOutput) &&
                workspace.workspaceId == exclusiveworkspaceId) {
                continue;
            }

            if (!wallpapers.contains(workspace.desktopWallpaper)) {
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

    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);

    bool update = false;
    for (WallpaperOutputConfig &outputConfig : m_wallpaperConfig) {
        if (outputConfig.workspaces.isEmpty()) {
            continue;
        }

        const WallpaperWorkspaceConfig refConfig = outputConfig.workspaces.constFirst();
        for (int i = 0; i < workspace->count(); i++) {
            const int workspaceId = workspace->modelAt(i)->id();
            if (!outputConfig.containsWorkspace(workspaceId)) {
                WallpaperWorkspaceConfig workspaceConfig;
                workspaceConfig.desktopWallpaper = refConfig.desktopWallpaper;
                workspaceConfig.desktopWallpapertype = refConfig.desktopWallpapertype;
                workspaceConfig.workspaceId = workspaceId;
                workspaceConfig.enable = true;
                outputConfig.workspaces.append(workspaceConfig);
                update = true;
            }
        }
    }

    if (update) {
        Helper::instance()->config()->setWallpaperConfig(wallpaperConfigToJsonString());
        Q_EMIT updateWallpaper();
    }
}

void WallpaperManager::removeOutputWallpaper(wlr_output *output)
{
    for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
        if (m_wallpaperConfig[i].outputName == getOutputId(output)) {
            WallpaperOutputConfig &outputConfig = m_wallpaperConfig[i];
            outputConfig.enable = false;
            for (WallpaperWorkspaceConfig &workspace : outputConfig.workspaces) {
                workspace.enable = false;
            }

            QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
            if (!globalWallpapers.contains(outputConfig.lockscreenWallpaper)) {
                Helper::instance()->m_wallpaperNotifierInterfaceV1->sendRemove(outputConfig.lockscreenWallpaper);
            }
            for (const WallpaperWorkspaceConfig &workspace : std::as_const(outputConfig.workspaces)) {
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

    return QStringLiteral("");
}

QString WallpaperManager::currentLockScreenWallpaper(WOutput *output)
{
    for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
        if (m_wallpaperConfig[i].outputName == getOutputId(output->nativeHandle())) {
            return m_wallpaperConfig[i].lockscreenWallpaper;
        }
    }

    return QStringLiteral("");
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

    qCCritical(lcTlWallpaper) << "Failed find wallpaper type for" << wallpaper;
    return TreelandWallpaperInterfaceV1::Image;
}

void WallpaperManager::onWallpaperAdded(TreelandWallpaperInterfaceV1 *interface)
{
    auto *output = interface->wOutput();
    if (!output)
        return;

    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);
    for (int i = 0; i < m_wallpaperConfig.size(); ++i) {
        if (m_wallpaperConfig[i].outputName == getOutputId(output->nativeHandle())) {
            WallpaperOutputConfig outputConfig = m_wallpaperConfig[i];
            interface->sendChanged(TreelandWallpaperInterfaceV1::Lockscreen, outputConfig.lockScreenWallpapertype, outputConfig.lockscreenWallpaper);
            for (WallpaperWorkspaceConfig workspaceConfig : std::as_const(outputConfig.workspaces)) {
                if (workspaceConfig.workspaceId == workspace->currentIndex()) {
                    interface->sendChanged(TreelandWallpaperInterfaceV1::Desktop, workspaceConfig.desktopWallpapertype, workspaceConfig.desktopWallpaper);
                    break;
                }
            }
            break;
        }
    }
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
    auto *output = interface->wOutput();
    if (!output)
        return;

    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    setOutputWallpaper(output->nativeHandle(),
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
    auto *output = interface->wOutput();
    if (!output)
        return;

    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    setOutputWallpaper(output->nativeHandle(),
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

void WallpaperManager::onWallpaperNotifierBound(wl_resource *resource)
{
    QMap<QString, TreelandWallpaperInterfaceV1::WallpaperType> globalWallpapers = globalValidWallpaper(nullptr, -1);
    QMapIterator<QString, TreelandWallpaperInterfaceV1::WallpaperType> i(globalWallpapers);
    while (i.hasNext()) {
        i.next();
        Helper::instance()->m_wallpaperNotifierInterfaceV1->sendAddForResource(resource, static_cast<TreelandWallpaperInterfaceV1::WallpaperType>(i.value()), i.key());
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
