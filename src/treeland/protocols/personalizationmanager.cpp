// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalizationmanager.h"

#include "personalization_manager_impl.h"

#include <qwsignalconnector.h>
#include <qwglobal.h>
#include <qwdisplay.h>

#include <QHash>

QW_USE_NAMESPACE

class TreeLandPersonalizationManagerPrivate : public QWObjectPrivate
{
public:
    TreeLandPersonalizationManagerPrivate(treeland_personalization_manager_v1 *handle, bool isOwner, TreeLandPersonalizationManager *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &TreeLandPersonalizationManagerPrivate::on_destroy);
        sc.connect(&handle->events.window_context_created, this,
                   &TreeLandPersonalizationManagerPrivate::on_window_context_created);
        sc.connect(&handle->events.wallpaper_context_created, this,
                   &TreeLandPersonalizationManagerPrivate::on_wallpaper_context_created);
    }
    ~TreeLandPersonalizationManagerPrivate() {
        if (!m_handle)
            return;
        destroy();
    }

    inline void destroy() {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        Q_EMIT q_func()->beforeDestroy(q_func());
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_window_context_created(void *);
    void on_wallpaper_context_created(void *);

    static QHash<void*, TreeLandPersonalizationManager*> map;
    QW_DECLARE_PUBLIC(TreeLandPersonalizationManager)
    QWSignalConnector sc;
};
QHash<void*, TreeLandPersonalizationManager*> TreeLandPersonalizationManagerPrivate::map;

void TreeLandPersonalizationManagerPrivate::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void TreeLandPersonalizationManagerPrivate::on_window_context_created(void *data)
{
    if (auto *p = reinterpret_cast<personalization_window_context_v1*>(data)) {
        Q_EMIT q_func()->windowContextCreated(PersonalizationWindowContext::from(p));
    }
}

void TreeLandPersonalizationManagerPrivate::on_wallpaper_context_created(void *data)
{
    if (auto *p = reinterpret_cast<personalization_wallpaper_context_v1*>(data)) {
        Q_EMIT q_func()->wallpaperContextCreated(PersonalizationWallpaperContext::from(p));
    }
}

TreeLandPersonalizationManager::TreeLandPersonalizationManager(treeland_personalization_manager_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new TreeLandPersonalizationManagerPrivate(handle, isOwner, this))
{

}

TreeLandPersonalizationManager *TreeLandPersonalizationManager::get(treeland_personalization_manager_v1 *handle)
{
    return TreeLandPersonalizationManagerPrivate::map.value(handle);
}

TreeLandPersonalizationManager *TreeLandPersonalizationManager::from(treeland_personalization_manager_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new TreeLandPersonalizationManager(handle, false);
}

TreeLandPersonalizationManager *TreeLandPersonalizationManager::create(QWDisplay *display)
{
    auto *handle = treeland_personalization_manager_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new TreeLandPersonalizationManager(handle, true);
}

void TreeLandPersonalizationManager::onSendUserWallpapers(personalization_wallpaper_context_v1 *wallpaper, const QStringList& wallpapers)
{
    personalization_wallpaper_v1_send_wallpapers(wallpaper, wallpapers);
}
