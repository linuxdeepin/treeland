// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// treeland_output_uuid_v1 is defined by treeland-output-uuid-unstable-v1 and only
// referenced by the head protocol, so the uuid header must come first: it is what
// declares treeland_output_uuid_v1_interface, which the head header uses.
#include "qwayland-treeland-output-uuid-unstable-v1.h"
#include "qwayland-treeland-output-uuid-head-unstable-v1.h"
#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QScreen>
#include <QString>
#include <QTimer>
#include <QWaylandClientExtension>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

constexpr int kUuidManagerVersion = 1;
constexpr int kWlrOutputManagerVersion = 4;
constexpr int kDefaultTimeoutMs = 2000;

// treeland reports QUuid::WithoutBraces: 32 hex digits and no dashes.
constexpr int kExpectedUuidLength = 32;

const QLatin1String kSourceOutput("wl_output");
const QLatin1String kSourceHead("zwlr_output_head_v1");

struct OutputEntry
{
    QString source;
    QString name;
    bool enabled = true;
    QString uuid;
};

bool entryLess(const OutputEntry &lhs, const OutputEntry &rhs)
{
    if (lhs.source != rhs.source) {
        return lhs.source < rhs.source;
    }
    return lhs.name < rhs.name;
}

wl_output *wlOutputForScreen(QScreen *screen)
{
    auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
    return native ? native->output() : nullptr;
}

} // namespace

// Wraps one treeland_output_uuid_v1 object. The compositor emits the uuid event
// once right after the object is created, and again whenever it assigns a
// different uuid to the output.
class OutputUuidV1
    : public QObject
    , public QtWayland::treeland_output_uuid_v1
{
    Q_OBJECT
public:
    OutputUuidV1(struct ::treeland_output_uuid_v1 *object,
                 const QString &source,
                 const QString &name,
                 bool enabled,
                 QObject *parent)
        : QObject(parent)
        , QtWayland::treeland_output_uuid_v1(object)
        , m_source(source)
        , m_name(name)
        , m_enabled(enabled)
    {
    }

    QString source() const
    {
        return m_source;
    }
    QString name() const
    {
        return m_name;
    }
    bool enabled() const
    {
        return m_enabled;
    }

Q_SIGNALS:
    void uuidReceived(OutputUuidV1 *self, const QString &uuid);

protected:
    void treeland_output_uuid_v1_uuid(const QString &uuid) override
    {
        if (m_reported) {
            return;
        }
        m_reported = true;
        Q_EMIT uuidReceived(this, uuid);
    }

private:
    QString m_source;
    QString m_name;
    bool m_enabled = true;
    bool m_reported = false;
};

class OutputUuidManagerV1
    : public QWaylandClientExtensionTemplate<OutputUuidManagerV1>
    , public QtWayland::treeland_output_uuid_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidManagerV1()
        : QWaylandClientExtensionTemplate<OutputUuidManagerV1>(kUuidManagerVersion)
    {
    }

    void instantiate()
    {
        initialize();
    }
};

class OutputUuidHeadManagerV1
    : public QWaylandClientExtensionTemplate<OutputUuidHeadManagerV1>
    , public QtWayland::treeland_output_uuid_head_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidHeadManagerV1()
        : QWaylandClientExtensionTemplate<OutputUuidHeadManagerV1>(kUuidManagerVersion)
    {
    }

    void instantiate()
    {
        initialize();
    }
};

// Keeps the name and enabled state of one zwlr_output_head_v1 so the head can be
// identified in the report. Heads are advertised even when the output is off, so
// they cover outputs that have no live wl_output global.
class HeadTracker : public QtWayland::zwlr_output_head_v1
{
public:
    explicit HeadTracker(struct ::zwlr_output_head_v1 *object)
        : QtWayland::zwlr_output_head_v1(object)
    {
    }

    ~HeadTracker() override
    {
        if (!m_finished && isInitialized()) {
            release();
        }
    }

    QString name;
    bool enabled = false;
    bool finished = false;

protected:
    void zwlr_output_head_v1_name(const QString &name) override
    {
        this->name = name;
    }
    void zwlr_output_head_v1_enabled(int32_t enabled) override
    {
        this->enabled = enabled != 0;
    }
    void zwlr_output_head_v1_finished() override
    {
        m_finished = true;
        finished = true;
        release();
    }

private:
    bool m_finished = false;
};

class HeadCollector
    : public QWaylandClientExtensionTemplate<HeadCollector>
    , public QtWayland::zwlr_output_manager_v1
{
    Q_OBJECT
public:
    struct Head
    {
        QString name;
        bool enabled = false;
        struct ::zwlr_output_head_v1 *object = nullptr;
    };

    explicit HeadCollector()
        : QWaylandClientExtensionTemplate<HeadCollector>(kWlrOutputManagerVersion)
    {
    }

    void instantiate()
    {
        initialize();
    }

    QList<Head> heads() const
    {
        QList<Head> result;
        for (const auto &tracker : m_heads) {
            if (tracker->finished || tracker->name.isEmpty()) {
                continue;
            }
            result.append({tracker->name, tracker->enabled, tracker->object()});
        }
        return result;
    }

Q_SIGNALS:
    void headsChanged();

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override
    {
        m_heads.push_back(std::make_unique<HeadTracker>(head));
    }

    void zwlr_output_manager_v1_done(uint32_t serial) override
    {
        Q_UNUSED(serial);
        m_heads.erase(std::remove_if(m_heads.begin(),
                                     m_heads.end(),
                                     [](const std::unique_ptr<HeadTracker> &tracker) {
                                         return tracker->finished;
                                     }),
                      m_heads.end());
        Q_EMIT headsChanged();
    }

private:
    std::vector<std::unique_ptr<HeadTracker>> m_heads;
};

class UuidProbe : public QObject
{
    Q_OBJECT
public:
    explicit UuidProbe(int timeoutMs, QObject *parent = nullptr)
        : QObject(parent)
        , m_timeoutMs(timeoutMs)
    {
        m_manager.setParent(this);
        m_headManager.setParent(this);
        m_headCollector.setParent(this);

        connect(&m_manager, &OutputUuidManagerV1::activeChanged, this, &UuidProbe::onStateChanged);
        connect(&m_headManager,
                &OutputUuidHeadManagerV1::activeChanged,
                this,
                &UuidProbe::onStateChanged);
        connect(&m_headCollector, &HeadCollector::activeChanged, this, &UuidProbe::onStateChanged);
        connect(&m_headCollector, &HeadCollector::headsChanged, this, &UuidProbe::onHeadsChanged);
        connect(&m_timeout, &QTimer::timeout, this, &UuidProbe::onTimeout);
    }

    void start()
    {
        m_manager.instantiate();
        m_headManager.instantiate();
        m_headCollector.instantiate();
        m_timeout.setSingleShot(true);
        m_timeout.start(m_timeoutMs);
    }

private:
    void onStateChanged()
    {
        maybeQuery();
    }

    void onHeadsChanged()
    {
        m_headsEnumerated = true;
        maybeQuery();
    }

    void maybeQuery()
    {
        if (m_queried || !m_headsEnumerated) {
            return;
        }
        if (!m_manager.isActive() && !m_headManager.isActive()) {
            return;
        }
        query();
    }

    void query()
    {
        m_queried = true;

        if (m_manager.isActive()) {
            const auto screens = QGuiApplication::screens();
            for (QScreen *screen : screens) {
                auto *output = wlOutputForScreen(screen);
                if (!output) {
                    continue;
                }
                addQuery(m_manager.get_uuid(output), kSourceOutput, screen->name(), true);
            }
        } else {
            qWarning() << "treeland_output_uuid_manager_v1 is not available";
        }

        if (m_headManager.isActive()) {
            const auto heads = m_headCollector.heads();
            for (const HeadCollector::Head &head : heads) {
                addQuery(m_headManager.get_uuid(head.object),
                         kSourceHead,
                         head.name,
                         head.enabled);
            }
        } else {
            qWarning() << "treeland_output_uuid_head_manager_v1 is not available";
        }

        if (m_pending == 0) {
            qWarning() << "No output to query.";
            finish();
        }
    }

    void addQuery(struct ::treeland_output_uuid_v1 *object,
                  const QString &source,
                  const QString &name,
                  bool enabled)
    {
        if (!object) {
            return;
        }
        auto *probe = new OutputUuidV1(object, source, name, enabled, this);
        connect(probe, &OutputUuidV1::uuidReceived, this, &UuidProbe::onUuidReceived);
        ++m_pending;
    }

    void onUuidReceived(OutputUuidV1 *self, const QString &uuid)
    {
        m_entries.append({self->source(), self->name(), self->enabled(), uuid});
        self->deleteLater();
        if (--m_pending == 0) {
            finish();
        }
    }

    void onTimeout()
    {
        if (!m_queried) {
            qWarning() << "Timed out waiting for the compositor; reporting what is available.";
            query();
            return;
        }
        qWarning() << "Timed out waiting for" << m_pending << "uuid event(s).";
        finish();
    }

    void finish()
    {
        m_timeout.stop();
        report();
        QGuiApplication::quit();
    }

    QString uuidFor(const QString &source, const QString &name) const
    {
        for (const OutputEntry &entry : m_entries) {
            if (entry.source == source && entry.name == name) {
                return entry.uuid;
            }
        }
        return {};
    }

    void report() const
    {
        QList<OutputEntry> entries = m_entries;
        std::sort(entries.begin(), entries.end(), entryLess);

        for (const OutputEntry &entry : entries) {
            if (entry.uuid.length() != kExpectedUuidLength) {
                qWarning().nospace() << "unexpected uuid " << entry.uuid << " for " << entry.name;
            }
            qInfo().nospace() << entry.source.leftJustified(20) << entry.name.leftJustified(10)
                              << (entry.enabled ? "enabled  " : "disabled ") << entry.uuid;
        }

        reportConsistency(entries);
    }

    // Both entry points must report the same uuid for the same physical output,
    // including outputs that are disabled and only have a head.
    void reportConsistency(const QList<OutputEntry> &entries) const
    {
        int compared = 0;
        for (const OutputEntry &entry : entries) {
            if (entry.source != kSourceOutput) {
                continue;
            }
            const QString headUuid = uuidFor(kSourceHead, entry.name);
            if (headUuid.isEmpty()) {
                continue;
            }
            ++compared;
            if (headUuid == entry.uuid) {
                qInfo().nospace() << "consistent: " << entry.name;
            } else {
                qWarning().nospace() << "mismatch: " << entry.name << " wl_output=" << entry.uuid
                                     << " head=" << headUuid;
            }
        }

        qInfo().nospace() << compared << " output(s) compared across both entry points, "
                          << m_entries.size() << " uuid(s) reported.";
    }

    int m_timeoutMs = kDefaultTimeoutMs;
    bool m_queried = false;
    bool m_headsEnumerated = false;
    int m_pending = 0;
    QTimer m_timeout;
    OutputUuidManagerV1 m_manager;
    OutputUuidHeadManagerV1 m_headManager;
    HeadCollector m_headCollector;
    QList<OutputEntry> m_entries;
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Query the stable per-output uuid exposed by treeland-output-uuid-unstable-v1, "
        "through both the wl_output and the zwlr_output_head_v1 entry points.");
    parser.addHelpOption();
    QCommandLineOption timeoutOption(QStringList() << "t"
                                                   << "timeout",
                                     "Milliseconds to wait for the compositor.",
                                     "timeout",
                                     QString::number(kDefaultTimeoutMs));
    parser.addOption(timeoutOption);
    parser.process(app);

    bool ok = false;
    const int timeoutMs = parser.value(timeoutOption).toInt(&ok);

    UuidProbe probe(ok ? timeoutMs : kDefaultTimeoutMs);
    probe.start();

    return app.exec();
}

#include "main.moc"
