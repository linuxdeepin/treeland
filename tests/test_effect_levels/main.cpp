// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dconfig/effectlevel.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <DConfig>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

namespace {

class MemoryConfigBackend : public DTK_CORE_NAMESPACE::DConfigBackend
{
public:
    explicit MemoryConfigBackend(QString backendName)
        : m_name(std::move(backendName))
    {
    }

    bool load(const QString &) override { return true; }
    bool isValid() const override { return true; }
    QStringList keyList() const override { return m_values.keys(); }
    QVariant value(const QString &key, const QVariant &fallback) const override
    {
        return m_values.contains(key) ? m_values.value(key) : fallback;
    }
    void setValue(const QString &key, const QVariant &value) override { m_values.insert(key, value); }
    void reset(const QString &key) override { m_values.remove(key); }
    bool isDefaultValue(const QString &key) const override { return !m_values.contains(key); }
    bool isReadOnly(const QString &) const override { return false; }
    QString name() const override { return m_name; }

private:
    QString m_name;
    QVariantMap m_values;
};

QJsonObject loadMeta(const QString &fileName)
{
    QFile file(QStringLiteral(TREELAND_DCONFIG_DIR "/") + fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

template<typename Config>
Config *createMemoryConfig(const QString &name, QObject *parent)
{
    auto *config = Config::create(new MemoryConfigBackend(name),
                                  QStringLiteral("org.deepin.dde.treeland"),
                                  QString(),
                                  parent);
    if (!QTest::qWaitFor([config] { return config->isInitializeSucceeded(); }, 3000))
        qWarning() << "Memory DConfig initialization timed out for" << name;
    return config;
}

} // namespace

class EffectLevelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void schemaDefaultsAndFlags();
    void levelParsingAndThresholdEvaluation();
    void globalLevelAndUserThresholdHotEdits();
};

void EffectLevelTest::schemaDefaultsAndFlags()
{
    QVERIFY(!QFile::exists(QStringLiteral(TREELAND_DCONFIG_DIR "/org.deepin.dde.treeland.effect.json")));
    QVERIFY(!QFile::exists(QStringLiteral(TREELAND_DCONFIG_DIR "/org.deepin.dde.treeland.animation.json")));

    const QJsonObject globalMeta = loadMeta(QStringLiteral("org.deepin.dde.treeland.json"));
    const QJsonObject userMeta = loadMeta(QStringLiteral("org.deepin.dde.treeland.user.json"));

    QVERIFY(!globalMeta.isEmpty());
    QVERIFY(!userMeta.isEmpty());
    QCOMPARE(globalMeta.value(QStringLiteral("magic")).toString(), QStringLiteral("dsg.config.meta"));
    QCOMPARE(globalMeta.value(QStringLiteral("version")).toString(), QStringLiteral("1.0"));
    QCOMPARE(userMeta.value(QStringLiteral("magic")).toString(), QStringLiteral("dsg.config.meta"));
    QCOMPARE(userMeta.value(QStringLiteral("version")).toString(), QStringLiteral("1.0"));

    const QJsonObject globalContents = globalMeta.value(QStringLiteral("contents")).toObject();
    const QJsonObject levelEntry = globalContents.value(QStringLiteral("effectLevel")).toObject();
    QCOMPARE(levelEntry.value(QStringLiteral("value")).toString(), QStringLiteral("high"));
    QCOMPARE(levelEntry.value(QStringLiteral("flags")).toArray(), QJsonArray{ QStringLiteral("global") });

    const QJsonObject userContents = userMeta.value(QStringLiteral("contents")).toObject();
    QVERIFY(!userContents.contains(QStringLiteral("effectLevel")));
    QVERIFY(userContents.value(QStringLiteral("glassEnabled")).toObject().value(QStringLiteral("value")).isBool());
    QVERIFY(userContents.value(QStringLiteral("lockscreenWallpaperDimEnabled"))
                .toObject()
                .value(QStringLiteral("value"))
                .isBool());

    const QStringList animKeys = {
        QStringLiteral("lockscreen"),
        QStringLiteral("windowOpen"),
        QStringLiteral("windowClose"),
        QStringLiteral("launchpadOpenFullscreen"),
        QStringLiteral("launchpadCloseFullscreen"),
        QStringLiteral("layerShellOpen"),
        QStringLiteral("layerShellClose"),
        QStringLiteral("maximize"),
        QStringLiteral("fullscreen"),
        QStringLiteral("restoreWindow"),
        QStringLiteral("minimize"),
        QStringLiteral("restoreMinimized"),
        QStringLiteral("showDesktop"),
        QStringLiteral("restoreShowDesktop"),
        QStringLiteral("launchpadCover"),
        QStringLiteral("wallpaperScale"),
        QStringLiteral("workspaceSwitch"),
        QStringLiteral("workspaceVisibility"),
        QStringLiteral("workspaceBounce"),
        QStringLiteral("taskSwitcher"),
        QStringLiteral("multitaskview"),
        QStringLiteral("outputRotation"),
        QStringLiteral("mirrorOutputRotation"),
        QStringLiteral("wallpaperCrossFade"),
        QStringLiteral("dockPreview"),
        QStringLiteral("captureToolbar"),
        QStringLiteral("edgeTilePreview"),
    };
    const QStringList effectKeys = {
        QStringLiteral("windowCornerRadiusEnabled"),
        QStringLiteral("windowShadowEnabled"),
        QStringLiteral("windowBorderEnabled"),
        QStringLiteral("windowTransparencyEnabled"),
        QStringLiteral("backgroundEffectEnabled"),
    };

    for (const QString &key : animKeys) {
        QVERIFY2(userContents.contains(key), qPrintable(key));
        const QJsonObject entry = userContents.value(key).toObject();
        QCOMPARE(static_cast<qlonglong>(entry.value(QStringLiteral("value")).toInt(-1)),
                 qlonglong(EffectLevel::High));
        QCOMPARE(entry.value(QStringLiteral("flags")).toArray(), QJsonArray{ QStringLiteral("user-public") });
    }
    for (const QString &key : effectKeys) {
        QVERIFY2(userContents.contains(key), qPrintable(key));
        const QJsonObject entry = userContents.value(key).toObject();
        QCOMPARE(static_cast<qlonglong>(entry.value(QStringLiteral("value")).toInt(-1)),
                 qlonglong(EffectLevel::Medium));
        QCOMPARE(entry.value(QStringLiteral("flags")).toArray(), QJsonArray{ QStringLiteral("user-public") });
    }
}

void EffectLevelTest::levelParsingAndThresholdEvaluation()
{
    bool valid = false;
    QCOMPARE(parseEffectLevel(QStringLiteral("none"), &valid), EffectLevel::None);
    QVERIFY(valid);
    QCOMPARE(effectLevelName(EffectLevel::None), QStringLiteral("none"));

    QCOMPARE(parseEffectLevel(QStringLiteral("low"), &valid), EffectLevel::Low);
    QVERIFY(valid);
    QCOMPARE(effectLevelName(EffectLevel::Low), QStringLiteral("low"));

    QCOMPARE(parseEffectLevel(QStringLiteral("medium"), &valid), EffectLevel::Medium);
    QVERIFY(valid);
    QCOMPARE(effectLevelName(EffectLevel::Medium), QStringLiteral("medium"));

    QCOMPARE(parseEffectLevel(QStringLiteral("high"), &valid), EffectLevel::High);
    QVERIFY(valid);
    QCOMPARE(effectLevelName(EffectLevel::High), QStringLiteral("high"));

    QCOMPARE(parseEffectLevel(QStringLiteral("invalid-tier"), &valid), EffectLevel::High);
    QVERIFY(!valid);
    QCOMPARE(parseEffectLevel(QString(), &valid), EffectLevel::High);
    QVERIFY(!valid);

    // Threshold 0 (None) is always disabled, even at High.
    QVERIFY(!effectEnabledForLevel(EffectLevel::None, EffectLevel::None));
    QVERIFY(!effectEnabledForLevel(EffectLevel::High, EffectLevel::None));

    // Threshold 1 (Low) enables at Low, Medium, High.
    QVERIFY(!effectEnabledForLevel(EffectLevel::None, EffectLevel::Low));
    QVERIFY(effectEnabledForLevel(EffectLevel::Low, EffectLevel::Low));
    QVERIFY(effectEnabledForLevel(EffectLevel::Medium, EffectLevel::Low));
    QVERIFY(effectEnabledForLevel(EffectLevel::High, EffectLevel::Low));

    // Threshold 2 (Medium) enables at Medium, High.
    QVERIFY(!effectEnabledForLevel(EffectLevel::None, EffectLevel::Medium));
    QVERIFY(!effectEnabledForLevel(EffectLevel::Low, EffectLevel::Medium));
    QVERIFY(effectEnabledForLevel(EffectLevel::Medium, EffectLevel::Medium));
    QVERIFY(effectEnabledForLevel(EffectLevel::High, EffectLevel::Medium));

    // Threshold 3 (High) enables at High only.
    QVERIFY(!effectEnabledForLevel(EffectLevel::None, EffectLevel::High));
    QVERIFY(!effectEnabledForLevel(EffectLevel::Low, EffectLevel::High));
    QVERIFY(!effectEnabledForLevel(EffectLevel::Medium, EffectLevel::High));
    QVERIFY(effectEnabledForLevel(EffectLevel::High, EffectLevel::High));

    // Out-of-range thresholds safely disable the item.
    QVERIFY(!effectEnabledForLevel(EffectLevel::High, -1));
    QVERIFY(!effectEnabledForLevel(EffectLevel::High, 99));
}

void EffectLevelTest::globalLevelAndUserThresholdHotEdits()
{
    QObject scope;
    auto *global = createMemoryConfig<TreelandConfig>(QStringLiteral("global"), &scope);
    auto *userA = createMemoryConfig<TreelandUserConfig>(QStringLiteral("userA"), &scope);
    auto *userB = createMemoryConfig<TreelandUserConfig>(QStringLiteral("userB"), &scope);
    QVERIFY(global->isInitializeSucceeded());
    QVERIFY(userA->isInitializeSucceeded());
    QVERIFY(userB->isInitializeSucceeded());

    auto currentLevel = [global] { return parseEffectLevel(global->effectLevel()); };

    // Default system level is high: animations (High) and effects (Medium) are enabled.
    QCOMPARE(global->effectLevel(), QStringLiteral("high"));
    QCOMPARE(userA->windowOpen(), qlonglong(EffectLevel::High));
    QCOMPARE(userA->edgeTilePreview(), qlonglong(EffectLevel::High));
    QCOMPARE(userA->windowCornerRadiusEnabled(), qlonglong(EffectLevel::Medium));
    QCOMPARE(userA->backgroundEffectEnabled(), qlonglong(EffectLevel::Medium));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowOpen()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->backgroundEffectEnabled()));

    // Switch system level to medium: animations (High) off, effects (Medium) on.
    QSignalSpy levelSpy(global, &TreelandConfig::effectLevelChanged);
    global->setEffectLevel(QStringLiteral("medium"));
    QCOMPARE(levelSpy.count(), 1);
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->windowOpen()));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->taskSwitcher()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowCornerRadiusEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowShadowEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowBorderEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowTransparencyEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->backgroundEffectEnabled()));

    // Hot-edit per-user thresholds independently of other users.
    QSignalSpy openSpy(userA, &TreelandUserConfig::windowOpenChanged);
    userA->setWindowOpen(EffectLevel::Medium);
    QCOMPARE(openSpy.count(), 1);
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->windowOpen()));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userB->windowOpen()));

    userA->setBackgroundEffectEnabled(EffectLevel::High);
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->backgroundEffectEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userB->backgroundEffectEnabled()));

    // Switch system level to low: items requiring Medium or High are disabled.
    global->setEffectLevel(QStringLiteral("low"));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->windowOpen()));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->windowCornerRadiusEnabled()));

    // Setting a per-user item to Low enables it at low, while system none disables it.
    userB->setWindowCornerRadiusEnabled(EffectLevel::Low);
    QVERIFY(effectEnabledForLevel(currentLevel(), userB->windowCornerRadiusEnabled()));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->windowCornerRadiusEnabled()));

    global->setEffectLevel(QStringLiteral("none"));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userB->windowCornerRadiusEnabled()));

    // Threshold None stays disabled even at system high.
    userA->setWindowShadowEnabled(EffectLevel::None);
    global->setEffectLevel(QStringLiteral("high"));
    QVERIFY(!effectEnabledForLevel(currentLevel(), userA->windowShadowEnabled()));
    QVERIFY(effectEnabledForLevel(currentLevel(), userA->backgroundEffectEnabled()));

    // glassEnabled remains an independent boolean in TreelandUserConfig.
    userA->setGlassEnabled(true);
    global->setEffectLevel(QStringLiteral("none"));
    QVERIFY(userA->glassEnabled());
}

QTEST_GUILESS_MAIN(EffectLevelTest)

#include "main.moc"
