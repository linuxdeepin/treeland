// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlroutputmanager.h"

#include "qwayland-wlr-output-management-unstable-v1.h"

#include <QDebug>

#include <algorithm>
#include <vector>

#include <wayland-client-core.h>

struct ModeEntry {
    struct ::zwlr_output_mode_v1 *object = nullptr;
    QSize size;
    int refresh = 0; // mHz
    bool preferred = false;
};

// Wraps one zwlr_output_mode_v1 and fills a ModeEntry.
class ModeTracker : public QtWayland::zwlr_output_mode_v1
{
public:
    ModeTracker(struct ::zwlr_output_mode_v1 *obj, ModeEntry *entry)
        : QtWayland::zwlr_output_mode_v1(obj)
        , m_entry(entry)
    {
    }

    ~ModeTracker() override
    {
        if (!m_finished && isInitialized())
            release();
    }

protected:
    void zwlr_output_mode_v1_size(int32_t width, int32_t height) override
    {
        m_entry->size = QSize(width, height);
    }
    void zwlr_output_mode_v1_refresh(int32_t refresh) override
    {
        m_entry->refresh = refresh;
    }
    void zwlr_output_mode_v1_preferred() override
    {
        m_entry->preferred = true;
    }
    void zwlr_output_mode_v1_finished() override
    {
        m_finished = true;
        m_entry->object = nullptr;
        release();
    }

private:
    ModeEntry *m_entry = nullptr;
    bool m_finished = false;
};

// Wraps one zwlr_output_head_v1 and collects all its properties.
class HeadTracker : public QtWayland::zwlr_output_head_v1
{
public:
    HeadTracker(struct ::zwlr_output_head_v1 *obj)
        : QtWayland::zwlr_output_head_v1(obj)
    {
    }

    ~HeadTracker() override
    {
        if (!m_finished && isInitialized())
            release();
    }

    QString name;
    QString description;
    bool enabled = false;
    QPoint position;
    int transform = 0; // wl_output_transform
    double scale = 1.0;
    QSize physicalSize;
    std::vector<std::unique_ptr<ModeEntry>> modes;
    std::vector<std::unique_ptr<ModeTracker>> modeTrackers;
    int currentModeIndex = -1;
    bool finished = false;

protected:
    void zwlr_output_head_v1_name(const QString &name) override { this->name = name; }
    void zwlr_output_head_v1_description(const QString &desc) override { this->description = desc; }
    void zwlr_output_head_v1_physical_size(int32_t w, int32_t h) override
    {
        physicalSize = QSize(w, h);
    }
    void zwlr_output_head_v1_mode(struct ::zwlr_output_mode_v1 *mode) override
    {
        auto entry = std::make_unique<ModeEntry>();
        entry->object = mode;
        modeTrackers.push_back(std::make_unique<ModeTracker>(mode, entry.get()));
        modes.push_back(std::move(entry));
    }
    void zwlr_output_head_v1_enabled(int32_t e) override { enabled = e != 0; }
    void zwlr_output_head_v1_current_mode(struct ::zwlr_output_mode_v1 *mode) override
    {
        for (size_t i = 0; i < modes.size(); ++i) {
            if (modes[i]->object == mode) {
                currentModeIndex = static_cast<int>(i);
                return;
            }
        }
    }
    void zwlr_output_head_v1_position(int32_t x, int32_t y) override { position = QPoint(x, y); }
    void zwlr_output_head_v1_transform(int32_t t) override { transform = t; }
    void zwlr_output_head_v1_scale(wl_fixed_t s) override { scale = wl_fixed_to_double(s); }
    void zwlr_output_head_v1_finished() override
    {
        m_finished = true;
        finished = true;
        release();
    }
    void zwlr_output_head_v1_make(const QString &) override { /* not needed */ }
    void zwlr_output_head_v1_model(const QString &) override { /* not needed */ }
    void zwlr_output_head_v1_serial_number(const QString &) override { /* not needed */ }
    void zwlr_output_head_v1_adaptive_sync(uint32_t) override { /* not needed */ }

private:
    bool m_finished = false;
};

// Wraps a zwlr_output_configuration_v1 and emits a signal on completion.
class ConfigWatcher : public QObject, public QtWayland::zwlr_output_configuration_v1
{
    Q_OBJECT
public:
    ConfigWatcher(struct ::zwlr_output_configuration_v1 *obj, QObject *parent)
        : QObject(parent)
        , QtWayland::zwlr_output_configuration_v1(obj)
    {
    }

Q_SIGNALS:
    void result(bool ok);

protected:
    void zwlr_output_configuration_v1_succeeded() override
    {
        Q_EMIT result(true);
        destroy();
        deleteLater();
    }
    void zwlr_output_configuration_v1_failed() override
    {
        Q_EMIT result(false);
        destroy();
        deleteLater();
    }
    void zwlr_output_configuration_v1_cancelled() override
    {
        Q_EMIT result(false);
        destroy();
        deleteLater();
    }
};

// ---------------------------------------------------------------------------
// WlrOutputManagerPrivate
// ---------------------------------------------------------------------------

class WlrOutputManagerPrivate
{
public:
    std::vector<std::unique_ptr<HeadTracker>> heads;
    uint32_t serial = 0;
    bool finished = false;
};

// ---------------------------------------------------------------------------
// WlrOutputManager
// ---------------------------------------------------------------------------

WlrOutputManager::WlrOutputManager()
    : QWaylandClientExtensionTemplate<WlrOutputManager>(4)
    , d(std::make_unique<WlrOutputManagerPrivate>())
{
}

WlrOutputManager::~WlrOutputManager() = default;

void WlrOutputManager::instantiate()
{
    initialize();
}

QVector<WlrOutputManager::Head> WlrOutputManager::heads() const
{
    QVector<Head> result;
    for (const auto &ht : d->heads) {
        if (ht->name.isEmpty())
            continue;
        Head h;
        h.name = ht->name;
        h.description = ht->description;
        h.enabled = ht->enabled;
        h.position = ht->position;
        h.transform = ht->transform;
        h.scale = ht->scale;
        h.physicalSize = ht->physicalSize;
        // Remap the index while skipping finished modes.
        int newIdx = 0;
        h.currentMode = -1;
        for (size_t i = 0; i < ht->modes.size(); ++i) {
            const auto &m = ht->modes[i];
            if (!m->object)
                continue;
            if (static_cast<int>(i) == ht->currentModeIndex)
                h.currentMode = newIdx;
            h.modes.append({m->size, m->refresh, m->preferred});
            ++newIdx;
        }
        result.append(h);
    }
    return result;
}


uint32_t WlrOutputManager::serial() const { return d->serial; }
void WlrOutputManager::applyConfig(const QString &headName,
                                   bool enabled,
                                   const QPoint &position,
                                   const QSize &modeSize,
                                   int refresh,
                                   int transform,
                                   double scale)
{
    if (!isActive() || d->heads.empty() || d->finished) {
        qWarning() << "WlrOutputManager: applyConfig skipped (manager inactive/finished or no heads)";
        return;
    }

    struct ::zwlr_output_configuration_v1 *configObj = create_configuration(d->serial);
    if (!configObj)
        return;

    auto *watcher = new ConfigWatcher(configObj, this);
    connect(watcher, &ConfigWatcher::result, this, &WlrOutputManager::applyFinished);

    for (const auto &ht : d->heads) {
        if (ht->name.isEmpty())
            continue;
        if (ht->name == headName) {
            if (!enabled) {
                watcher->disable_head(ht->object());
            } else {
                auto *cfgHead = watcher->enable_head(ht->object());
                QtWayland::zwlr_output_configuration_head_v1 cfg(cfgHead);
                cfg.set_position(position.x(), position.y());
                bool modeSet = false;
                for (const auto &mode : ht->modes) {
                    if (mode->object && mode->size == modeSize && (refresh <= 0 || mode->refresh == refresh)) {
                        cfg.set_mode(mode->object);
                        modeSet = true;
                        break;
                    }
                }
                if (!modeSet && !modeSize.isEmpty()) {
                    cfg.set_custom_mode(modeSize.width(), modeSize.height(), refresh);
                } else if (!modeSet && ht->currentModeIndex >= 0
                           && static_cast<size_t>(ht->currentModeIndex) < ht->modes.size()) {
                    auto *mObj = ht->modes[ht->currentModeIndex]->object;
                    if (mObj)
                        cfg.set_mode(mObj);
                }
                cfg.set_transform(transform);
                cfg.set_scale(wl_fixed_from_double(scale));
            }
        } else if (ht->enabled) {
            auto *cfgHead = watcher->enable_head(ht->object());
            QtWayland::zwlr_output_configuration_head_v1 cfg(cfgHead);
            cfg.set_position(ht->position.x(), ht->position.y());
            if (ht->currentModeIndex >= 0 && static_cast<size_t>(ht->currentModeIndex) < ht->modes.size()) {
                auto *mObj = ht->modes[ht->currentModeIndex]->object;
                if (mObj)
                    cfg.set_mode(mObj);
            }
            cfg.set_transform(ht->transform);
            cfg.set_scale(wl_fixed_from_double(ht->scale));
        } else {
            watcher->disable_head(ht->object());
        }
    }
    watcher->apply();
}

void WlrOutputManager::zwlr_output_manager_v1_head(struct ::zwlr_output_head_v1 *head)
{
    d->heads.push_back(std::make_unique<HeadTracker>(head));
}

void WlrOutputManager::zwlr_output_manager_v1_done(uint32_t serial)
{
    d->serial = serial;
    d->heads.erase(std::remove_if(d->heads.begin(), d->heads.end(),
                                  [](const auto &ht) { return ht->finished; }),
                   d->heads.end());
    Q_EMIT headsChanged();
}

void WlrOutputManager::zwlr_output_manager_v1_finished()
{
    d->finished = true;
}

#include "wlroutputmanager.moc"