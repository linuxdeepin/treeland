// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct WQmlCreatorData;
struct Q_DECL_HIDDEN WQmlCreatorDelegateData {
    QPointer<QObject> object;
    QWeakPointer<WQmlCreatorData> data;
};

class WAbstractCreatorComponent;
struct Q_DECL_HIDDEN WQmlCreatorData {
    QObject *owner;
    QList<std::pair<WAbstractCreatorComponent*, QWeakPointer<WQmlCreatorDelegateData>>> delegateDatas;
    QJSValue properties;
};

class WQmlCreator;
class WAbstractCreatorComponentPrivate;
class WAYLIB_SERVER_EXPORT WAbstractCreatorComponent : public QObject, public WObject
{
    W_DECLARE_PRIVATE(WAbstractCreatorComponent)
    Q_OBJECT
    Q_PROPERTY(WQmlCreator* creator READ creator WRITE setCreator NOTIFY creatorChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WAbstractCreatorComponent(QObject *parent = nullptr);

    virtual QSharedPointer<WQmlCreatorDelegateData> add(QSharedPointer<WQmlCreatorData> data) = 0;
    virtual void remove(QSharedPointer<WQmlCreatorDelegateData> data);
    virtual void remove(QSharedPointer<WQmlCreatorData> data);
    virtual QList<QSharedPointer<WQmlCreatorDelegateData>> datas() const;

    WQmlCreator *creator() const;
    void setCreator(WQmlCreator *newCreator);

Q_SIGNALS:
    void creatorChanged();

protected:
    virtual void creatorChange(WQmlCreator *oldCreator, WQmlCreator *newCreator);
    void notifyCreatorObjectAdded(WQmlCreator *creator, QObject *object, const QJSValue &initialProperties);
    void notifyCreatorObjectRemoved(WQmlCreator *creator, QObject *object, const QJSValue &initialProperties);
};

class WQmlCreatorPrivate;
class WAYLIB_SERVER_EXPORT WQmlCreator : public QObject, public WObject
{
    friend class WQmlCreatorComponent;
    W_DECLARE_PRIVATE(WQmlCreator)
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    QML_NAMED_ELEMENT(DynamicCreator)

public:
    explicit WQmlCreator(QObject *parent = nullptr);
    ~WQmlCreator();

    const QList<WAbstractCreatorComponent *> &delegates() const;

    int count() const;

public Q_SLOTS:
    // for model
    void add(const QJSValue &initialProperties);
    void add(QObject *owner, const QJSValue &initialProperties);
    bool removeByOwner(QObject *owner);
    void clear(bool notify);

    bool removeIf(QJSValue function);
    QObject *getIf(QJSValue function) const;
    QObject *getIf(WAbstractCreatorComponent *delegate, QJSValue function) const;

    // for delegate
    QObject *get(int index) const;
    QObject *get(WAbstractCreatorComponent *delegate, int index) const;
    QObject *getByOwner(QObject *owner) const;
    QObject *getByOwner(WAbstractCreatorComponent *delegate, QObject *owner) const;

Q_SIGNALS:
    void objectAdded(WAbstractCreatorComponent *component, QObject *object,
                     const QJSValue &initialProperties);
    void objectRemoved(WAbstractCreatorComponent *component, QObject *object,
                       const QJSValue &initialProperties);
    void countChanged();

private:
    void destroy(QSharedPointer<WQmlCreatorData> data);
    bool remove(int index);

    int indexOfOwner(QObject *owner) const;
    int indexOf(QJSValue function) const;

    void addDelegate(WAbstractCreatorComponent *delegate);
    void createAll(WAbstractCreatorComponent *delegate);
    void removeDelegate(WAbstractCreatorComponent *delegate);

    friend class WAbstractCreatorComponent;
};

WAYLIB_SERVER_END_NAMESPACE
