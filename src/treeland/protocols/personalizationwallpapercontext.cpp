// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager_impl.h"
#include "personalizationmanager.h"

#include <qwglobal.h>
#include <qwoutput.h>
#include <qwsignalconnector.h>

#include <QHash>

QW_USE_NAMESPACE

class PersonalizationWallpaperContextPrivate : public QWObjectPrivate
{
public:
    PersonalizationWallpaperContextPrivate(personalization_wallpaper_context_v1 *handle,
                                           bool isOwner,
                                           PersonalizationWallpaperContext *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &PersonalizationWallpaperContextPrivate::on_destroy);
        sc.connect(&handle->events.commit, this,
                   &PersonalizationWallpaperContextPrivate::on_commit);
        sc.connect(&handle->events.get_wallpapers, this,
                   &PersonalizationWallpaperContextPrivate::on_get_wallpapers);
    }

    ~PersonalizationWallpaperContextPrivate()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner)
            personalization_wallpaper_context_v1_destroy(q_func()->handle());
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_commit(void *);
    void on_get_wallpapers(void *);

    static QHash<void *, PersonalizationWallpaperContext *> map;
    QW_DECLARE_PUBLIC(PersonalizationWallpaperContext)
    QWSignalConnector sc;
};

QHash<void *, PersonalizationWallpaperContext *> PersonalizationWallpaperContextPrivate::map;

void PersonalizationWallpaperContextPrivate::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void PersonalizationWallpaperContextPrivate::on_commit(void *data)
{
    Q_EMIT q_func()->commit(reinterpret_cast<personalization_wallpaper_context_v1*>(data));
}

void PersonalizationWallpaperContextPrivate::on_get_wallpapers(void *data)
{
    Q_EMIT q_func()->getWallpapers(reinterpret_cast<personalization_wallpaper_context_v1*>(data));
}

PersonalizationWallpaperContext::PersonalizationWallpaperContext(
    personalization_wallpaper_context_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new PersonalizationWallpaperContextPrivate(handle, isOwner, this))
{
}

PersonalizationWallpaperContext *PersonalizationWallpaperContext::get(
    personalization_wallpaper_context_v1 *handle)
{
    return PersonalizationWallpaperContextPrivate::map.value(handle);
}

PersonalizationWallpaperContext *PersonalizationWallpaperContext::from(
    personalization_wallpaper_context_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new PersonalizationWallpaperContext(handle, false);
}

PersonalizationWallpaperContext *PersonalizationWallpaperContext::create(
    personalization_wallpaper_context_v1 *handle)
{
    if (!handle)
        return nullptr;
    return new PersonalizationWallpaperContext(handle, true);
}
