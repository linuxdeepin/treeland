// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "surface/surfacecontainer.h"

#include <forward_list>

class SurfaceWrapper;
class Workspace;

class WorkspaceModel : public SurfaceListModel
{
    friend class Workspace;
    friend class MultitaskviewSurfaceModel;
    Q_OBJECT
    Q_PROPERTY(int id READ id CONSTANT FINAL)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(bool opaque READ opaque WRITE setOpaque NOTIFY opaqueChanged FINAL)

    QML_ELEMENT

public:
    explicit WorkspaceModel(QObject *parent,
                            int id,
                            std::forward_list<SurfaceWrapper *> activedSurfaceHistory);

    QString name() const;
    void setName(const QString &newName);

    int id() const;

    bool visible() const;
    void setVisible(bool visible);

    bool opaque() const;
    void setOpaque(bool opaque);

    void addSurface(SurfaceWrapper *surface) override;
    void removeSurface(SurfaceWrapper *surface) override;

    Q_INVOKABLE SurfaceWrapper *latestActiveSurface() const;
    Q_INVOKABLE SurfaceWrapper *activePenultimateWindow() const;
    Q_INVOKABLE SurfaceWrapper *findNextActivedSurface() const;
    void pushActivedSurface(SurfaceWrapper *surface);
    void removeActivedSurface(SurfaceWrapper *surface);
    int findActivedSurfaceHistoryIndex(SurfaceWrapper *surface) const;

Q_SIGNALS:
    void nameChanged();
    void indexChanged();
    void visibleChanged();
    void opaqueChanged();

private:
    QString m_name;
    int m_id = -1;
    bool m_visible = false;
    bool m_opaque = true;
    std::forward_list<SurfaceWrapper *> m_activedSurfaceHistory;
};
