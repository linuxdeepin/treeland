// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QList>
#include <QQmlComponent>
#include <QQmlContext>

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
class WAYLIB_SERVER_EXPORT WAbstractCreatorComponent : public QObject
{
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

    WQmlCreator *m_creator = nullptr;
};

class WAYLIB_SERVER_EXPORT WQmlCreatorDataWatcher : public WAbstractCreatorComponent
{
    Q_OBJECT
    QML_NAMED_ELEMENT(DynamicCreatorDataWatcher)
public:
    explicit WQmlCreatorDataWatcher(QObject *parent = nullptr);

Q_SIGNALS:
    void added(QObject *owner, const QJSValue &initialProperties);
    void removed(QObject *owner, const QJSValue &initialProperties);

private:
    QSharedPointer<WQmlCreatorDelegateData> add(QSharedPointer<WQmlCreatorData> data) override;
    void remove(QSharedPointer<WQmlCreatorData> data) override;
};

class WAYLIB_SERVER_EXPORT WQmlCreatorComponent : public WAbstractCreatorComponent
{
    Q_OBJECT
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate FINAL REQUIRED)
    Q_PROPERTY(QObject* parent READ parent WRITE setParent NOTIFY parentChanged FINAL)
    Q_PROPERTY(QString chooserRole READ chooserRole WRITE setChooserRole NOTIFY chooserRoleChanged FINAL)
    Q_PROPERTY(QVariant chooserRoleValue READ chooserRoleValue WRITE setChooserRoleValue NOTIFY chooserRoleValueChanged FINAL)
    Q_PROPERTY(QVariantMap contextProperties WRITE setContextProperties FINAL)
    Q_PROPERTY(bool autoDestroy READ autoDestroy WRITE setAutoDestroy NOTIFY autoDestroyChanged FINAL)
    QML_NAMED_ELEMENT(DynamicCreatorComponent)
    Q_CLASSINFO("DefaultProperty", "delegate")

public:
    explicit WQmlCreatorComponent(QObject *parent = nullptr);
    ~WQmlCreatorComponent();

    bool checkByChooser(const QJSValue &properties) const;

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *component);

    QString chooserRole() const;
    void setChooserRole(const QString &newChooserRole);

    QVariant chooserRoleValue() const;
    void setChooserRoleValue(QVariant newChooserRoleValue);

    void setContextProperties(const QVariantMap &properties);

    bool autoDestroy() const;
    void setAutoDestroy(bool newAutoDestroy);

    QObject *parent() const;
    void setParent(QObject *newParent);

    QList<QSharedPointer<WQmlCreatorDelegateData>> datas() const override;

    Q_INVOKABLE void destroyObject(QObject *object);

Q_SIGNALS:
    void parentChanged();
    void chooserRoleChanged();
    void chooserRoleValueChanged();
    void autoDestroyChanged();

    void objectAdded(QObject *object, const QJSValue &initialProperties);
    void objectRemoved(QObject *object, const QJSValue &initialProperties);

private:
    QSharedPointer<WQmlCreatorDelegateData> add(QSharedPointer<WQmlCreatorData> data) override;
    void remove(QSharedPointer<WQmlCreatorData> data) override;

    void destroy(QSharedPointer<WQmlCreatorDelegateData> data);
    void remove(QSharedPointer<WQmlCreatorDelegateData> data) override;

    void clear();
    void reset();
    void create(QSharedPointer<WQmlCreatorDelegateData> data);
    Q_SLOT void create(QSharedPointer<WQmlCreatorDelegateData> data, QObject *parent, const QJSValue &initialProperties);

    QQmlComponent *m_delegate = nullptr;
    QObject *m_parent = nullptr;
    QString m_chooserRole;
    QVariant m_chooserRoleValue;
    bool m_autoDestroy = true;
    QList<QQmlContext::PropertyPair> m_contextProperties;

    QList<QSharedPointer<WQmlCreatorDelegateData>> m_datas;
};

class WAYLIB_SERVER_EXPORT WQmlCreator : public QObject
{
    friend class WQmlCreatorComponent;
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    QML_NAMED_ELEMENT(DynamicCreator)

public:
    explicit WQmlCreator(QObject *parent = nullptr);
    ~WQmlCreator();

    QList<WAbstractCreatorComponent *> delegates() const;

    int count() const;

public Q_SLOTS:
    // for model
    void add(const QJSValue &initialProperties);
    void add(QObject *owner, const QJSValue &initialProperties);
    bool removeIf(QJSValue function);
    bool removeByOwner(QObject *owner);
    void clear(bool notify);

    // for delegate
    QObject *get(int index) const;
    QObject *get(WAbstractCreatorComponent *delegate, int index) const;
    QObject *getIf(QJSValue function) const;
    QObject *getIf(WAbstractCreatorComponent *delegate, QJSValue function) const;
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

    QList<WAbstractCreatorComponent*> m_delegates;
    QList<QSharedPointer<WQmlCreatorData>> m_datas;

    friend class WAbstractCreatorComponent;
};

WAYLIB_SERVER_END_NAMESPACE
