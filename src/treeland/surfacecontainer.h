// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "surfacewrapper.h"

#include <QAbstractListModel>
#include <QProperty>
#include <QPropertyNotifier>
#include <QQuickItem>

#include <functional>

Q_MOC_INCLUDE("rootsurfacecontainer.h")

template<typename T>
class ObjectListModel : public QAbstractListModel
{
public:
    explicit ObjectListModel(QByteArrayView objectName, QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_name(objectName.toByteArray())
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        if (parent.isValid())
            return 0;

        return m_objects.count();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_objects.count())
            return {};

        if (role == Qt::DisplayRole)
            return QVariant::fromValue(m_objects.at(index.row()));

        return {};
    }

    QMap<int, QVariant> itemData(const QModelIndex &index) const override
    {
        if (!index.isValid() || index.row() >= m_objects.count())
            return {};

        QMap<int, QVariant> data;
        data.insert(Qt::DisplayRole, QVariant::fromValue(m_objects.at(index.row())));
        return data;
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    }

    QHash<int, QByteArray> roleNames() const override { return { { Qt::DisplayRole, m_name } }; }

    bool addObject(T *object)
    {
        if (m_objects.contains(object))
            return false;

        beginInsertRows(QModelIndex(), m_objects.count(), m_objects.count());
        m_objects.append(object);
        endInsertRows();
        return true;
    }

    bool removeObject(T *object)
    {
        int index = m_objects.indexOf(object);
        if (index < 0)
            return false;
        beginRemoveRows({}, index, index);
        m_objects.removeAt(index);
        endRemoveRows();
        return true;
    }

    bool hasObject(T *object) const { return m_objects.contains(object); }

    const QList<T *> &objects() const { return m_objects; }

protected:
    QByteArray m_name;
    QList<T *> m_objects;
};

class SurfaceListModel : public ObjectListModel<SurfaceWrapper>
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit SurfaceListModel(QObject *parent = nullptr);

    virtual void addSurface(SurfaceWrapper *surface)
    {
        if (addObject(surface))
            emit surfaceAdded(surface);
    }

    virtual void removeSurface(SurfaceWrapper *surface)
    {
        if (removeObject(surface))
            emit surfaceRemoved(surface);
    }

    QHash<int, QByteArray> roleNames() const override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    inline const QList<SurfaceWrapper *> &surfaces() const { return objects(); }

    inline bool hasSurface(SurfaceWrapper *surface) const { return hasObject(surface); }

signals:
    void surfaceAdded(SurfaceWrapper *surface);
    void surfaceRemoved(SurfaceWrapper *surface);

private:
    using ObjectListModel<SurfaceWrapper>::addObject;
    using ObjectListModel<SurfaceWrapper>::removeObject;
    using ObjectListModel<SurfaceWrapper>::objects;
};

class SurfaceFilterModel : public SurfaceListModel
{
    Q_OBJECT
    QML_ANONYMOUS

public:
    explicit SurfaceFilterModel(SurfaceListModel *parent);

    inline SurfaceListModel *parent() const
    {
        auto op = QObject::parent();
        auto p = qobject_cast<SurfaceListModel *>(op);
        Q_ASSERT(p);
        return p;
    }

    void setFilter(std::function<bool(SurfaceWrapper *)> filter);

private:
    using SurfaceListModel::addSurface;
    using SurfaceListModel::removeSurface;

    void initForSourceSurface(SurfaceWrapper *surface);
    void makeBindingForSourceSurface(SurfaceWrapper *surface);
    void updateSurfaceVisibility(SurfaceWrapper *surface);

    std::function<bool(SurfaceWrapper *)> m_filter;

    struct Property
    {
        Property()
            : prop(false)
        {
        }

        template<typename F>
        inline void setBinding(F f)
        {
            prop.setBinding(f);
        }

        bool init = false;
        QProperty<bool> prop;
        QPropertyNotifier notifier;
    };

    std::map<SurfaceWrapper *, Property> m_properties;
};

class Output;
class RootSurfaceContainer;

class SurfaceContainer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(SurfaceListModel* model READ model CONSTANT FINAL)
    Q_PROPERTY(RootSurfaceContainer* root READ rootContainer CONSTANT FINAL)
    QML_ANONYMOUS

public:
    explicit SurfaceContainer(QQuickItem *parent = nullptr);
    explicit SurfaceContainer(SurfaceContainer *parent);
    ~SurfaceContainer() override;

    RootSurfaceContainer *rootContainer() const;
    SurfaceContainer *parentContainer() const;
    QList<SurfaceContainer *> subContainers() const;
    void setQmlEngine(QQmlEngine *engine);

    virtual void addSurface(SurfaceWrapper *surface);
    virtual void removeSurface(SurfaceWrapper *surface);

    virtual void addOutput(Output *output);
    virtual void removeOutput(Output *output);

    const QList<SurfaceWrapper *> &surfaces() const { return m_model->surfaces(); }

    SurfaceListModel *model() const { return m_model; }

signals:
    void surfaceAdded(SurfaceWrapper *surface);
    void surfaceRemoved(SurfaceWrapper *surface);

protected:
    friend class SurfaceWrapper;

    void geometryChange(const QRectF &newGeo, const QRectF &oldGeo) override;

    bool doAddSurface(SurfaceWrapper *surface, bool setContainer);
    bool doRemoveSurface(SurfaceWrapper *surface, bool setContainer);

    virtual void addBySubContainer(SurfaceContainer *sub, SurfaceWrapper *surface);
    virtual void removeBySubContainer(SurfaceContainer *sub, SurfaceWrapper *surface);

    virtual bool filterSurfaceGeometryChanged(SurfaceWrapper *surface,
                                              QRectF &newGeometry,
                                              const QRectF &oldGeometry);
    virtual bool filterSurfaceStateChange(SurfaceWrapper *surface,
                                          SurfaceWrapper::State newState,
                                          SurfaceWrapper::State oldState);

    SurfaceListModel *m_model = nullptr;
};
