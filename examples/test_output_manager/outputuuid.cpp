// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "outputuuid.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWaylandClientExtension>

#include <algorithm>

namespace {

constexpr int kUuidManagerVersion = 1;
constexpr int kWlrOutputManagerVersion = 4;
constexpr int kDefaultTimeoutMs = 2000;

// treeland reports QUuid::WithoutBraces: 32 hex digits and no dashes.
constexpr int kExpectedUuidLength = 32;

const QLatin1String kSourceOutput("wl_output");
const QLatin1String kSourceHead("zwlr_output_head_v1");

wl_output *wlOutputForScreen(QScreen *screen)
{
    auto *native = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
    return native ? native->output() : nullptr;
}

} // namespace

OutputUuidV1::OutputUuidV1(struct ::treeland_output_uuid_v1 *object,
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

void OutputUuidV1::treeland_output_uuid_v1_uuid(const QString &uuid)
{
    if (m_reported) {
        return;
    }
    m_reported = true;
    m_uuid = uuid;
    Q_EMIT uuidReceived(this, uuid);
}

OutputUuidManagerV1::OutputUuidManagerV1()
    : QWaylandClientExtensionTemplate<OutputUuidManagerV1>(kUuidManagerVersion)
{
}

void OutputUuidManagerV1::instantiate()
{
    initialize();
}

OutputUuidHeadManagerV1::OutputUuidHeadManagerV1()
    : QWaylandClientExtensionTemplate<OutputUuidHeadManagerV1>(kUuidManagerVersion)
{
}

void OutputUuidHeadManagerV1::instantiate()
{
    initialize();
}

HeadTracker::HeadTracker(struct ::zwlr_output_head_v1 *object)
    : QtWayland::zwlr_output_head_v1(object)
{
}

HeadTracker::~HeadTracker()
{
    if (!m_finished && isInitialized()) {
        release();
    }
}

void HeadTracker::zwlr_output_head_v1_name(const QString &name)
{
    this->name = name;
}

void HeadTracker::zwlr_output_head_v1_enabled(int32_t enabled)
{
    this->enabled = enabled != 0;
}

void HeadTracker::zwlr_output_head_v1_finished()
{
    m_finished = true;
    finished = true;
    release();
}

HeadCollector::HeadCollector()
    : QWaylandClientExtensionTemplate<HeadCollector>(kWlrOutputManagerVersion)
{
}

void HeadCollector::instantiate()
{
    initialize();
}

QList<HeadCollector::Head> HeadCollector::heads() const
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

void HeadCollector::zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head)
{
    m_heads.push_back(std::make_unique<HeadTracker>(head));
}

void HeadCollector::zwlr_output_manager_v1_done(uint32_t serial)
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

UuidProbe::UuidProbe(int timeoutMs, QObject *parent)
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

void UuidProbe::start()
{
    m_manager.instantiate();
    m_headManager.instantiate();
    m_headCollector.instantiate();
    m_timeout.setSingleShot(true);
    m_timeout.start(m_timeoutMs);
}

void UuidProbe::onStateChanged()
{
    maybeQuery();
}

void UuidProbe::onHeadsChanged()
{
    m_headsEnumerated = true;
    maybeQuery();
}

void UuidProbe::maybeQuery()
{
    if (m_queried || !m_headsEnumerated) {
        return;
    }
    if (!m_manager.isActive() && !m_headManager.isActive()) {
        return;
    }
    query();
}

void UuidProbe::query()
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

void UuidProbe::addQuery(struct ::treeland_output_uuid_v1 *object,
                         const QString &source,
                         const QString &name,
                         bool enabled)
{
    if (!object) {
        return;
    }
    auto *probe = new OutputUuidV1(object, source, name, enabled, this);
    connect(probe, &OutputUuidV1::uuidReceived, this, &UuidProbe::onUuidReceived);
    m_entries.append(probe);
    ++m_pending;
}

void UuidProbe::onUuidReceived(OutputUuidV1 *self, const QString &uuid)
{
    Q_UNUSED(self);
    Q_UNUSED(uuid);
    if (--m_pending == 0) {
        finish();
    }
}

void UuidProbe::onTimeout()
{
    if (!m_queried) {
        qWarning() << "Timed out waiting for the compositor; reporting what is available.";
        query();
        return;
    }
    qWarning() << "Timed out waiting for" << m_pending << "uuid event(s).";
    finish();
}

void UuidProbe::finish()
{
    m_timeout.stop();
    report();
    QGuiApplication::quit();
}

void UuidProbe::report() const
{
    QList<OutputUuidV1 *> entries = m_entries;
    std::sort(entries.begin(), entries.end(), [](OutputUuidV1 *lhs, OutputUuidV1 *rhs) {
        if (lhs->source() != rhs->source()) {
            return lhs->source() < rhs->source();
        }
        return lhs->name() < rhs->name();
    });

    for (const OutputUuidV1 *entry : entries) {
        if (entry->uuid().length() != kExpectedUuidLength) {
            qWarning().nospace() << "unexpected uuid " << entry->uuid() << " for "
                                 << entry->name();
        }
        qInfo().nospace() << entry->source().leftJustified(20) << entry->name().leftJustified(10)
                          << (entry->enabled() ? "enabled  " : "disabled ") << entry->uuid();
    }

    reportConsistency();
}

// Both entry points must report the same uuid for the same physical output,
// including outputs that are disabled and only have a head.
void UuidProbe::reportConsistency() const
{
    auto uuidFor = [this](const QString &source, const QString &name) {
        for (const OutputUuidV1 *entry : m_entries) {
            if (entry->source() == source && entry->name() == name) {
                return entry->uuid();
            }
        }
        return QString();
    };

    int compared = 0;
    for (const OutputUuidV1 *entry : m_entries) {
        if (entry->source() != kSourceOutput) {
            continue;
        }
        const QString headUuid = uuidFor(kSourceHead, entry->name());
        if (headUuid.isEmpty()) {
            continue;
        }
        ++compared;
        if (headUuid == entry->uuid()) {
            qInfo().nospace() << "consistent: " << entry->name();
        } else {
            qWarning().nospace() << "mismatch: " << entry->name() << " wl_output=" << entry->uuid()
                                 << " head=" << headUuid;
        }
    }

    qInfo().nospace() << compared << " output(s) compared across both entry points, "
                      << m_entries.size() << " uuid(s) reported.";
}
