// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>
#include <private/wglobal_p.h>

#include <QList>
#include <QQmlComponent>
#include <QQmlContext>

WAYLIB_SERVER_BEGIN_NAMESPACE

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

class Q_DECL_HIDDEN WAbstractCreatorComponentPrivate : public WObjectPrivate
{
public:
    WAbstractCreatorComponentPrivate(WAbstractCreatorComponent *qq)
        : WObjectPrivate(qq) {}

    W_DECLARE_PUBLIC(WAbstractCreatorComponent)

    WQmlCreator *creator = nullptr;
};

class Q_DECL_HIDDEN WQmlCreatorPrivate : public WObjectPrivate
{
public:
    WQmlCreatorPrivate(WQmlCreator *qq)
        : WObjectPrivate(qq) {}

    W_DECLARE_PUBLIC(WQmlCreator)

    QList<WAbstractCreatorComponent*> delegates;
    QList<QSharedPointer<WQmlCreatorData>> datas;
};

WAYLIB_SERVER_END_NAMESPACE
