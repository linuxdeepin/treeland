// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wqmlcreator_p.h"
#include "wxdgsurface.h"

#include <QJSValue>
#include <QQuickItem>
#include <QQmlInfo>
#include <private/qqmlcomponent_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WAbstractCreatorComponent::WAbstractCreatorComponent(QObject *parent)
    : QObject(parent)
    , WObject(*new WAbstractCreatorComponentPrivate(this))
{

}

void WAbstractCreatorComponent::remove(QSharedPointer<WQmlCreatorDelegateData>)
{

}

void WAbstractCreatorComponent::remove(QSharedPointer<WQmlCreatorData>)
{

}

QList<QSharedPointer<WQmlCreatorDelegateData>> WAbstractCreatorComponent::datas() const
{
    return {};
}

WQmlCreator *WAbstractCreatorComponent::creator() const
{
    W_DC(WAbstractCreatorComponent);

    return d->creator;
}

void WAbstractCreatorComponent::setCreator(WQmlCreator *newCreator)
{
    W_D(WAbstractCreatorComponent);

    if (d->creator == newCreator)
        return;
    auto oldCreator = d->creator;
    d->creator = newCreator;
    creatorChange(oldCreator, newCreator);

    Q_EMIT creatorChanged();
}

void WAbstractCreatorComponent::creatorChange(WQmlCreator *oldCreator, WQmlCreator *newCreator)
{
    if (oldCreator)
        oldCreator->removeDelegate(this);

    if (newCreator)
        newCreator->addDelegate(this);
}

void WAbstractCreatorComponent::notifyCreatorObjectAdded(WQmlCreator *creator, QObject *object,
                                                         const QJSValue &initialProperties)
{
    Q_EMIT creator->objectAdded(this, object, initialProperties);
}

void WAbstractCreatorComponent::notifyCreatorObjectRemoved(WQmlCreator *creator, QObject *object,
                                                           const QJSValue &initialProperties)
{
    Q_EMIT creator->objectRemoved(this, object, initialProperties);
}

WQmlCreatorDataWatcher::WQmlCreatorDataWatcher(QObject *parent)
    : WAbstractCreatorComponent(parent)
{

}

QSharedPointer<WQmlCreatorDelegateData> WQmlCreatorDataWatcher::add(QSharedPointer<WQmlCreatorData> data)
{
    Q_EMIT added(data->owner, data->properties);
    return nullptr;
}

void WQmlCreatorDataWatcher::remove(QSharedPointer<WQmlCreatorData> data)
{
    Q_EMIT removed(data->owner, data->properties);
}

WQmlCreatorComponent::WQmlCreatorComponent(QObject *parent)
    : WAbstractCreatorComponent(parent)
{

}

WQmlCreatorComponent::~WQmlCreatorComponent()
{
    if (creator())
        creator()->removeDelegate(this);

    clear();
}

bool WQmlCreatorComponent::checkByChooser(const QJSValue &properties) const
{
    if (m_chooserRole.isEmpty())
        return true;
    return properties.property(m_chooserRole).toVariant() == m_chooserRoleValue;
}

QQmlComponent *WQmlCreatorComponent::delegate() const
{
    return m_delegate;
}

void WQmlCreatorComponent::setDelegate(QQmlComponent *component)
{
    if (m_delegate) {
        component->engine()->throwError(QJSValue::GenericError, "Property 'delegate' can only be assigned once.");
        return;
    }

    m_delegate = component;
}

QSharedPointer<WQmlCreatorDelegateData> WQmlCreatorComponent::add(QSharedPointer<WQmlCreatorData> data)
{
    if (!checkByChooser(data->properties))
        return nullptr;

    QSharedPointer<WQmlCreatorDelegateData> d(new WQmlCreatorDelegateData());
    d->data = data;
    m_datas << d;
    create(d);

    return d;
}

void WQmlCreatorComponent::remove(QSharedPointer<WQmlCreatorData> data)
{
    for (const auto &d : std::as_const(data->delegateDatas)) {
        if (d.first != this)
            continue;
        if (d.second)
            remove(d.second.toStrongRef());
    }
}

void WQmlCreatorComponent::destroy(QSharedPointer<WQmlCreatorDelegateData> data)
{
    if (data->object) {
        auto obj = data->object.get();
        data->object.clear();
        const QJSValue p = data->data.lock()->properties;
        Q_EMIT objectRemoved(obj, p);
        notifyCreatorObjectRemoved(creator(), obj, p);

        if (m_autoDestroy) {
            obj->setParent(nullptr);
            delete obj;
            obj = nullptr;
        }
    }
}

void WQmlCreatorComponent::remove(QSharedPointer<WQmlCreatorDelegateData> data)
{
    bool ok = m_datas.removeOne(data);
    Q_ASSERT(ok);
    destroy(data);
}

void WQmlCreatorComponent::clear()
{
    for (auto d : std::as_const(m_datas)) {
        d->data.lock()->delegateDatas.removeOne({this, d});
        destroy(d);
    }

    m_datas.clear();
}

void WQmlCreatorComponent::reset()
{
    clear();

    if (creator())
        creator()->createAll(this);
}

void WQmlCreatorComponent::create(QSharedPointer<WQmlCreatorDelegateData> data)
{
    auto parent = m_parent ? m_parent : QObject::parent();

    if (data->object)
        destroy(data);

    Q_ASSERT(data->data);
    Q_ASSERT(m_delegate);

    auto d = QQmlComponentPrivate::get(m_delegate);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (d->state.isCompletePending()) {
        QMetaObject::invokeMethod(this, "create", Qt::QueuedConnection, data, parent, data->data.lock()->properties);
#else
    if (d->state.completePending) {
        QMetaObject::invokeMethod(this, "create", Qt::QueuedConnection,
                                  Q_ARG(QSharedPointer<WQmlCreatorDelegateData>, data),
                                  Q_ARG(QObject *, parent),
                                  Q_ARG(const QJSValue &, data->data.lock()->properties));
#endif
    } else {
        create(data, parent, data->data.lock()->properties);
    }
}

void WQmlCreatorComponent::create(QSharedPointer<WQmlCreatorDelegateData> data, QObject *parent, const QJSValue &initialProperties)
{
    auto d = QQmlComponentPrivate::get(m_delegate);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    Q_ASSERT(!d->state.isCompletePending());
#else
    Q_ASSERT(!d->state.completePending);
#endif
    // Don't use QVariantMap instead of QJSValue, because initial properties may be
    // contains some QObject property , if that QObjects is destroyed in future,
    // the QVariantMap's property would not update, you will get a invalid QObject pointer
    // if you using it after it's destroyed, but the QJSValue will watching that QObjects,
    // you will get a null pointer if you using after it's destroyed.
    const auto tmp = qvariant_cast<QVariantMap>(initialProperties.toVariant());

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto context = new QQmlContext(qmlContext(this), this);
    context->setContextProperties(m_contextProperties);
    data->object = d->createWithProperties(parent, tmp, context);
    context->setParent(data->object);
#else
    // The `createWithInitialProperties` provided by QQmlComponent cannot set parent
    // during the creation process. use `setParent` will too late as the creation has
    // complete, lead to the windows of WOutputRenderWindow get empty...

    // for Qt 6.5 The createWithInitialProperties with the parent parameter provided by
    // QQmlComponentPrivate can solve this problem.
    // Qt 6.4 requires beginCreate -> setInitialProperties/setParent -> completeCreate

    QObject *rv = m_delegate->beginCreate(qmlContext(this));
    if (rv) {
        m_delegate->setInitialProperties(rv, tmp);
        rv->setParent(parent);
        if (auto item = qobject_cast<QQuickItem*>(rv))
            item->setParentItem(qobject_cast<QQuickItem*>(parent));
        m_delegate->completeCreate();
        if (!d->requiredProperties().empty()) {
            for (const auto &unsetRequiredProperty : std::as_const(d->requiredProperties())) {
                const QQmlError error = QQmlComponentPrivate::unsetRequiredPropertyToQQmlError(unsetRequiredProperty);
                qmlWarning(rv, error);
            }
            d->requiredProperties().clear();
            rv->deleteLater();
            rv = nullptr;
        }
    }
    data->object = rv;
#endif

    if (data->object) {
        Q_EMIT objectAdded(data->object, initialProperties);
        notifyCreatorObjectAdded(creator(), data->object, initialProperties);
    } else {
        qWarning() << "WQmlCreatorComponent::create failed" << "parent=" << parent << "initialProperties=" << tmp;
        for (auto e: d->state.errors)
            qWarning() << e.error;
    }
}

QObject *WQmlCreatorComponent::parent() const
{
    return m_parent;
}

void WQmlCreatorComponent::setParent(QObject *newParent)
{
    if (m_parent == newParent)
        return;
    m_parent = newParent;
    Q_EMIT parentChanged();
}

QList<QSharedPointer<WQmlCreatorDelegateData> > WQmlCreatorComponent::datas() const
{
    return m_datas;
}

// If autoDestroy is disabled, can using this interface to manual manager the object's life.
void WQmlCreatorComponent::destroyObject(QObject *object)
{
    object->deleteLater();
}

QString WQmlCreatorComponent::chooserRole() const
{
    return m_chooserRole;
}

void WQmlCreatorComponent::setChooserRole(const QString &newChooserRole)
{
    if (m_chooserRole == newChooserRole)
        return;
    m_chooserRole = newChooserRole;
    reset();

    Q_EMIT chooserRoleChanged();
}

QVariant WQmlCreatorComponent::chooserRoleValue() const
{
    return m_chooserRoleValue;
}

void WQmlCreatorComponent::setChooserRoleValue(QVariant newChooserRoleValue)
{
    // jsvalue assigned to qproperty aren't auto converted to QVariantMap, differs with invoke args
    if (newChooserRoleValue.canConvert<QJSValue>())
        newChooserRoleValue = newChooserRoleValue.value<QJSValue>().toVariant();
    if (m_chooserRoleValue == newChooserRoleValue)
        return;
    m_chooserRoleValue = newChooserRoleValue;
    reset();

    Q_EMIT chooserRoleValueChanged();
}

void WQmlCreatorComponent::setContextProperties(const QVariantMap &properties)
{
    m_contextProperties.clear();
    for (auto [k,v]: properties.asKeyValueRange()) {
        m_contextProperties.append(QQmlContext::PropertyPair{k,v});
    }
}

bool WQmlCreatorComponent::autoDestroy() const
{
    return m_autoDestroy;
}

void WQmlCreatorComponent::setAutoDestroy(bool newAutoDestroy)
{
    if (m_autoDestroy == newAutoDestroy)
        return;
    m_autoDestroy = newAutoDestroy;
    Q_EMIT autoDestroyChanged();
}

WQmlCreator::WQmlCreator(QObject *parent)
    : QObject{parent}
    , WObject(*new WQmlCreatorPrivate(this))
{

}

WQmlCreator::~WQmlCreator()
{
    clear(false);

    W_D(WQmlCreator);

    for (auto delegate : std::as_const(d->delegates)) {
        Q_ASSERT(delegate->creator() == this);
        delegate->setCreator(nullptr);
    }
}

const QList<WAbstractCreatorComponent*> &WQmlCreator::delegates() const
{
    W_DC(WQmlCreator);
    return d->delegates;
}

int WQmlCreator::count() const
{
    W_DC(WQmlCreator);
    return d->datas.size();
}

void WQmlCreator::add(const QJSValue &initialProperties)
{
    add(nullptr, initialProperties);
}

void WQmlCreator::add(QObject *owner, const QJSValue &initialProperties)
{
    QSharedPointer<WQmlCreatorData> data(new WQmlCreatorData());
    data->owner = owner;
    data->properties = initialProperties;

    W_D(WQmlCreator);

    for (auto delegate : std::as_const(d->delegates)) {
        if (auto d = delegate->add(data.toWeakRef()))
            data->delegateDatas.append({delegate, d});
    }

    d->datas << data;

    if (owner) {
        connect(owner, &QObject::destroyed, this, [this] {
            bool ok = removeByOwner(sender());
            Q_ASSERT(ok);
        });
    }

    Q_EMIT countChanged();
}

bool WQmlCreator::removeIf(QJSValue function)
{
    return remove(indexOf(function));
}

bool WQmlCreator::removeByOwner(QObject *owner)
{
    return remove(indexOfOwner(owner));
}

void WQmlCreator::clear(bool notify)
{
    W_D(WQmlCreator);

    if (d->datas.isEmpty())
        return;

    for (auto data : std::as_const(d->datas))
        destroy(data);

    d->datas.clear();

    if (notify)
        Q_EMIT countChanged();
}

QObject *WQmlCreator::get(int index) const
{
    W_DC(WQmlCreator);

    if (index < 0 || index >= d->datas.size())
        return nullptr;

    auto data = d->datas.at(index);
    if (data->delegateDatas.isEmpty())
        return nullptr;
    return data->delegateDatas.first().second.lock()->object.get();
}

QObject *WQmlCreator::get(WAbstractCreatorComponent *delegate, int index) const
{
    W_DC(WQmlCreator);

    if (index < 0 || index >= d->datas.size())
        return nullptr;

    auto data = d->datas.at(index);
    for (const auto &d : std::as_const(data->delegateDatas)) {
        if (d.first != delegate)
            continue;
        return d.second ? d.second.lock()->object.get() : nullptr;
    }

    return nullptr;
}

QObject *WQmlCreator::getIf(QJSValue function) const
{
    W_DC(WQmlCreator);

    for (auto delegate : std::as_const(d->delegates)) {
        auto obj = getIf(delegate, function);
        if (obj)
            return obj;
    }
    return nullptr;
}

QObject *WQmlCreator::getIf(WAbstractCreatorComponent *delegate, QJSValue function) const
{
    return get(delegate, indexOf(function));
}

QObject *WQmlCreator::getByOwner(QObject *owner) const
{
    W_DC(WQmlCreator);

    for (auto delegate : std::as_const(d->delegates)) {
        auto obj = getByOwner(delegate, owner);
        if (obj)
            return obj;
    }
    return nullptr;
}

QObject *WQmlCreator::getByOwner(WAbstractCreatorComponent *delegate, QObject *owner) const
{
    return get(delegate, indexOfOwner(owner));
}

void WQmlCreator::destroy(QSharedPointer<WQmlCreatorData> data)
{
    if (data->owner)
        data->owner->disconnect(this);

    W_D(WQmlCreator);

    for (auto delegate : std::as_const(d->delegates))
        delegate->remove(data);
}

bool WQmlCreator::remove(int index)
{
    W_D(WQmlCreator);

    if (index < 0 || index >= d->datas.size())
        return false;
    auto data = d->datas.takeAt(index);
    destroy(data);

    Q_EMIT countChanged();

    return true;
}

int WQmlCreator::indexOfOwner(QObject *owner) const
{
    W_DC(WQmlCreator);

    for (int i = 0; i < d->datas.size(); ++i) {
        if (d->datas.at(i)->owner == owner)
            return i;
    }

    return -1;
}

int WQmlCreator::indexOf(QJSValue function) const
{
    W_DC(WQmlCreator);

    for (int i = 0; i < d->datas.size(); ++i) {
        if (function.call({d->datas.at(i)->properties}).toBool())
            return i;
    }

    return -1;
}

void WQmlCreator::addDelegate(WAbstractCreatorComponent *delegate)
{
    W_D(WQmlCreator);

    Q_ASSERT(!d->delegates.contains(delegate));
    d->delegates.append(delegate);
    createAll(delegate);
}

void WQmlCreator::createAll(WAbstractCreatorComponent *delegate)
{
    W_D(WQmlCreator);

    for (const auto &data : std::as_const(d->datas)) {
        if (auto d = delegate->add(data))
            data->delegateDatas.append({delegate, d});
    }
}

void WQmlCreator::removeDelegate(WAbstractCreatorComponent *delegate)
{
    W_D(WQmlCreator);

    bool ok = d->delegates.removeOne(delegate);
    Q_ASSERT(ok);

    const auto datas = delegate->datas();
    for (auto d : datas) {
        bool ok = d->data.lock()->delegateDatas.removeOne({delegate, d});
        Q_ASSERT(ok);
    }
}

WAYLIB_SERVER_END_NAMESPACE
