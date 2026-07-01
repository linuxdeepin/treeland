// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "treelandwallpapermanagerclient.h"

#include <QGuiApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QSet>
#include <QMimeDatabase>
#include <QMimeType>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <qpa/qplatformnativeinterface.h>

#include <unistd.h>

static QScreen *findScreenByName(const QString &name)
{
    if (name.isEmpty())
        return nullptr;

    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }

    return nullptr;
}

static bool isImageFile(const QString &path)
{
    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    return mime.isValid() && mime.name().startsWith("image/");
}

static bool isVideoFile(const QString &path)
{
    static QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchContent);
    return mime.isValid() && mime.name().startsWith("video/");
}

static bool isFragmentShaderFile(const QString &path)
{
    return QFileInfo(path).suffix().compare(QStringLiteral("frag"), Qt::CaseInsensitive) == 0;
}

static QByteArray shaderSourceWithoutVersion(const QByteArray &source)
{
    QByteArray result;
    const QList<QByteArray> lines = source.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().startsWith("#version"))
            continue;

        result += line;
        result += '\n';
    }
    return result;
}

static QByteArray shaderBakeSource(const QByteArray &source)
{
    if (!source.contains("mainImage"))
        return source;

    QByteArray baked;
    baked += "#version 440\n";
    baked += "layout(location = 0) in vec2 qt_TexCoord0;\n";
    baked += "layout(location = 0) out vec4 fragColor;\n";
    baked += "layout(std140, binding = 0) uniform buf {\n";
    baked += "    mat4 qt_Matrix;\n";
    baked += "    float qt_Opacity;\n";
    baked += "    float iTime;\n";
    baked += "    vec2 iResolution;\n";
    baked += "    vec4 iMouse;\n";
    baked += "};\n";
    baked += shaderSourceWithoutVersion(source);
    baked += "\nvoid main() {\n";
    baked += "    vec2 fragCoord = vec2(qt_TexCoord0.x, 1.0 - qt_TexCoord0.y) * iResolution;\n";
    baked += "    mainImage(fragColor, fragCoord);\n";
    baked += "    fragColor *= qt_Opacity;\n";
    baked += "}\n";
    return baked;
}

static bool writeFileAtomically(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    if (file.write(data) != data.size())
        return false;

    if (!file.commit())
        return false;

    QFile::setPermissions(path,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadGroup
                              | QFileDevice::ReadOther);
    return true;
}

static bool ensureSharedDirectory(const QString &path)
{
    QDir dir;
    if (!dir.mkpath(path))
        return false;

    return QFile::setPermissions(path,
                                 QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                     | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                     | QFileDevice::ReadOther | QFileDevice::ExeOther);
}

static bool hasCurrentShaderPackageVersion(const QString &metadataPath)
{
    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() && doc.object()[QStringLiteral("packageVersion")].toInt() == 3;
}

static QString findQsbExecutable()
{
    const QString qsb = QStandardPaths::findExecutable(QStringLiteral("qsb"));
    if (!qsb.isEmpty())
        return qsb;

    const QString qt6Qsb = QStringLiteral("/usr/lib/qt6/bin/qsb");
    if (QFileInfo(qt6Qsb).isExecutable())
        return qt6Qsb;

    return QString();
}

static QString shaderPackageRoot()
{
    return QDir::temp().filePath(QStringLiteral("treeland-wallpaper-shader/%1").arg(getuid()));
}

static bool ensureShaderPackage(const QString &fragmentPath, QString *packagePath)
{
    QFile fragment(fragmentPath);
    if (!fragment.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open fragment shader:" << fragmentPath << fragment.errorString();
        return false;
    }

    const QByteArray source = fragment.readAll();
    const QByteArray hash = QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex();
    QDir root(shaderPackageRoot());
    if (!ensureSharedDirectory(root.absolutePath())
        || !ensureSharedDirectory(root.filePath(QString::fromLatin1(hash)))) {
        qCritical() << "Failed to create shader wallpaper cache directory:" << root.absolutePath();
        return false;
    }

    QDir package(root.filePath(QString::fromLatin1(hash)));
    const QString sourcePath = package.filePath(QStringLiteral("fragment.frag"));
    const QString bakePath = package.filePath(QStringLiteral("fragment.qt.frag"));
    const QString qsbPath = package.filePath(QStringLiteral("fragment.qsb"));
    const QString qsbTempPath = package.filePath(QStringLiteral("fragment.qsb.tmp"));
    const QString metadataPath = package.filePath(QStringLiteral("metadata.json"));

    if (!QFileInfo::exists(qsbPath) || !hasCurrentShaderPackageVersion(metadataPath)) {
        if (!writeFileAtomically(sourcePath, source)) {
            qCritical() << "Failed to write shader source:" << sourcePath;
            return false;
        }

        if (!writeFileAtomically(bakePath, shaderBakeSource(source))) {
            qCritical() << "Failed to write shader bake source:" << bakePath;
            return false;
        }

        const QString qsb = findQsbExecutable();
        if (qsb.isEmpty()) {
            qCritical() << "Cannot find qsb. Install qt6-shader-baker to import .frag wallpaper.";
            return false;
        }

        QFile::remove(qsbTempPath);

        QProcess process;
        process.setProgram(qsb);
        process.setArguments({
            QStringLiteral("--glsl"), QStringLiteral("100es,120,150"),
            QStringLiteral("--hlsl"), QStringLiteral("50"),
            QStringLiteral("--msl"), QStringLiteral("12"),
            QStringLiteral("-o"), qsbTempPath,
            bakePath,
        });
        process.start();
        if (!process.waitForFinished(30000)) {
            process.kill();
            process.waitForFinished();
            qCritical() << "Timed out compiling fragment shader with qsb:" << bakePath;
            QFile::remove(qsbTempPath);
            return false;
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            qCritical().noquote() << "Failed to compile fragment shader with qsb:"
                                  << QString::fromLocal8Bit(process.readAllStandardError());
            QFile::remove(qsbTempPath);
            return false;
        }

        if (!QFileInfo(qsbTempPath).isFile() || QFileInfo(qsbTempPath).size() <= 0) {
            qCritical() << "qsb did not produce a valid shader package output:" << qsbTempPath;
            QFile::remove(qsbTempPath);
            return false;
        }

        QFile::remove(qsbPath);
        if (!QFile::rename(qsbTempPath, qsbPath)) {
            qCritical() << "Failed to install compiled shader package output:" << qsbPath;
            QFile::remove(qsbTempPath);
            return false;
        }

        QJsonObject metadata;
        metadata[QStringLiteral("packageVersion")] = 3;
        metadata[QStringLiteral("type")] = QStringLiteral("shader");
        metadata[QStringLiteral("fragmentShader")] = QStringLiteral("fragment.qsb");
        metadata[QStringLiteral("sourceShader")] = QStringLiteral("fragment.frag");
        metadata[QStringLiteral("uniforms")] = QJsonObject {
            { QStringLiteral("iTime"), true },
            { QStringLiteral("iResolution"), true },
            { QStringLiteral("iMouse"), false },
        };
        if (!writeFileAtomically(metadataPath, QJsonDocument(metadata).toJson(QJsonDocument::Indented))) {
            qCritical() << "Failed to write shader wallpaper metadata:" << metadataPath;
            return false;
        }
    }

    *packagePath = package.absolutePath();
    return true;
}

static uint32_t
parseRole(const QString &role)
{
    using Role = QtWayland::treeland_wallpaper_v1::wallpaper_role;

    if (role == "desktop")
        return Role::wallpaper_role_desktop;
    if (role == "lockscreen")
        return Role::wallpaper_role_lockscreen;
    if (role == "both")
        return Role::wallpaper_role_desktop | Role::wallpaper_role_lockscreen;

    return Role::wallpaper_role_desktop; // default
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Treeland Wallpaper Client"));
    parser.addHelpOption();

    QCommandLineOption outputNameOption(
        QStringLiteral("output"),
        QStringLiteral("Wayland output name (e.g. eDP-1, HDMI-1). Same as wlr-randr output name."),
        QStringLiteral("output-name")
        );
    parser.addOption(outputNameOption);
    QCommandLineOption pathOption(
        QStringLiteral("path"),
        QStringLiteral("Wallpaper path (image, video, or fragment shader)."),
        QStringLiteral("file-path")
        );
    parser.addOption(pathOption);

    QCommandLineOption roleOption(
        QStringLiteral("role"),
        QStringLiteral("Wallpaper role: desktop | lockscreen | both."),
        QStringLiteral("role"),
        QStringLiteral("desktop")
        );
    parser.addOption(roleOption);
    parser.process(app);

    const QString outputName = parser.value(outputNameOption);
    QScreen *targetScreen = findScreenByName(outputName);

    if (!targetScreen) {
        qCritical().noquote()
        << "Cannot find QScreen for output:" << outputName
        << "\nAvailable outputs are:";
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens)
            qCritical() << " -" << screen->name();
        return EXIT_FAILURE;
    }

    const QString wallpaperPath = parser.value(pathOption);
    const QString roleString = parser.value(roleOption);

    if (wallpaperPath.isEmpty()) {
        qCritical() << "--path is required";
        return EXIT_FAILURE;
    }

    TreelandWallpaperManagerV1 *wallpaperManager = new TreelandWallpaperManagerV1;
    QObject::connect(wallpaperManager, &TreelandWallpaperManagerV1::activeChanged, [&] {
        if (wallpaperManager->isActive()) {
            if (!targetScreen) {
                qWarning() << "createWallpaper called with null QScreen";
                qApp->exit(EXIT_FAILURE);
                return;
            }

            auto *nativeInterface = qGuiApp->platformNativeInterface();
            if (!nativeInterface) {
                qCritical() << "No QPlatformNativeInterface available";
                qApp->exit(EXIT_FAILURE);
                return;
            }

            wl_output *wlOutput = static_cast<wl_output *>(
                nativeInterface->nativeResourceForScreen(
                    QByteArrayLiteral("output"), targetScreen));

            if (!wlOutput) {
                qCritical() << "Failed to get wl_output for screen:" << targetScreen->name();
                qApp->exit(EXIT_FAILURE);
                return;
            }

            auto *object = wallpaperManager->get_treeland_wallpaper(wlOutput, nullptr);
            if (!object) {
                qCritical() << "treeland_wallpaper_manager_v1 returned null object";
                qApp->exit(EXIT_FAILURE);
                return;
            }
            TreelandWallpaperV1 *wallpaper = new TreelandWallpaperV1(object);
            const uint32_t role = parseRole(roleString);
            if (isImageFile(wallpaperPath)) {
                wallpaper->set_image_source(wallpaperPath, role);
            } else if (isVideoFile(wallpaperPath)) {
                wallpaper->set_video_source(wallpaperPath, role);
            } else if (isFragmentShaderFile(wallpaperPath)) {
                if (wallpaper->version() < TreelandWallpaperManagerV1::InterfaceVersion) {
                    qCritical() << "Compositor wallpaper protocol version is" << wallpaper->version()
                                << "but shader wallpaper requires version"
                                << TreelandWallpaperManagerV1::InterfaceVersion
                                << ". Restart treeland with the updated treeland-protocols and compositor build.";
                    delete wallpaper;
                    delete wallpaperManager;
                    qApp->exit(EXIT_FAILURE);
                    return;
                }

                QString packagePath;
                if (!ensureShaderPackage(wallpaperPath, &packagePath)) {
                    delete wallpaper;
                    delete wallpaperManager;
                    qApp->exit(EXIT_FAILURE);
                    return;
                }

                wallpaper->set_shader_source(packagePath, role);
            } else {
                qCritical() << "Unsupported wallpaper file type:" << wallpaperPath;
                delete wallpaper;
                delete wallpaperManager;
                qApp->exit(EXIT_FAILURE);
                return;
            }
        }
    });
    wallpaperManager->instantiate();

    return app.exec();
}
