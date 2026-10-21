// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

// treeland_output_uuid_v1 is defined by treeland-output-uuid-unstable-v1 and only
// referenced by the head protocol, so the uuid header must come first: it is what
// declares treeland_output_uuid_v1_interface, which the head header uses.
#include "qwayland-treeland-output-uuid-unstable-v1.h"
#include "qwayland-treeland-output-uuid-head-unstable-v1.h"
#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QWaylandClientExtension>

#include <memory>
#include <vector>

// Wraps one treeland_output_uuid_v1 object. The compositor emits the uuid event
// once right after the object is created.
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
                 QObject *parent = nullptr);

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
    QString uuid() const
    {
        return m_uuid;
    }

Q_SIGNALS:
    void uuidReceived(OutputUuidV1 *self, const QString &uuid);

protected:
    void treeland_output_uuid_v1_uuid(const QString &uuid) override;

private:
    QString m_source;
    QString m_name;
    QString m_uuid;
    bool m_enabled = true;
    bool m_reported = false;
};

class OutputUuidManagerV1
    : public QWaylandClientExtensionTemplate<OutputUuidManagerV1>
    , public QtWayland::treeland_output_uuid_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidManagerV1();

    void instantiate();
};

class OutputUuidHeadManagerV1
    : public QWaylandClientExtensionTemplate<OutputUuidHeadManagerV1>
    , public QtWayland::treeland_output_uuid_head_manager_v1
{
    Q_OBJECT
public:
    explicit OutputUuidHeadManagerV1();

    void instantiate();
};

// Keeps the name and enabled state of one zwlr_output_head_v1 so the head can be
// identified in the report. Heads are advertised even when the output is off, so
// they cover outputs that have no live wl_output global.
class HeadTracker : public QtWayland::zwlr_output_head_v1
{
public:
    explicit HeadTracker(struct ::zwlr_output_head_v1 *object);
    ~HeadTracker() override;

    QString name;
    bool enabled = false;
    bool finished = false;

protected:
    void zwlr_output_head_v1_name(const QString &name) override;
    void zwlr_output_head_v1_enabled(int32_t enabled) override;
    void zwlr_output_head_v1_finished() override;

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

    explicit HeadCollector();

    void instantiate();
    QList<Head> heads() const;

Q_SIGNALS:
    void headsChanged();

protected:
    void zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head) override;
    void zwlr_output_manager_v1_done(uint32_t serial) override;

private:
    std::vector<std::unique_ptr<HeadTracker>> m_heads;
};

// One-shot diagnostic: prints the uuid of every output reachable through the
// wl_output and zwlr_output_head_v1 entry points, then quits the application.
class UuidProbe : public QObject
{
    Q_OBJECT
public:
    explicit UuidProbe(int timeoutMs, QObject *parent = nullptr);

    void start();

private:
    void onStateChanged();
    void onHeadsChanged();
    void maybeQuery();
    void query();
    void addQuery(struct ::treeland_output_uuid_v1 *object,
                  const QString &source,
                  const QString &name,
                  bool enabled);
    void onUuidReceived(OutputUuidV1 *self, const QString &uuid);
    void onTimeout();
    void finish();
    void report() const;
    void reportConsistency() const;

    int m_timeoutMs;
    bool m_queried = false;
    bool m_headsEnumerated = false;
    int m_pending = 0;
    QTimer m_timeout;
    OutputUuidManagerV1 m_manager;
    OutputUuidHeadManagerV1 m_headManager;
    HeadCollector m_headCollector;
    QList<OutputUuidV1 *> m_entries;
};
