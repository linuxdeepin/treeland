#pragma once

#include <QObject>

class FakeObject
{
public:
    explicit FakeObject() = default;
    virtual ~FakeObject() = default;

    qint64 value() const
    {
        return m_value;
    }

    void setValue(qint64 value)
    {
        m_value = value;
    }

private:
    qint64 m_value{ 0 };
};

class FakeWrapObject : public QObject
{
    Q_OBJECT
public:
    FakeWrapObject(QObject *parent = nullptr)
        : QObject(parent)
        , m_object(new FakeObject)
    {
    }

    ~FakeWrapObject() override
    {
        invalidate();
    }

    void invalidate()
    {
        delete m_object;
        m_object = nullptr;
        Q_EMIT aboutToBeInvalidated();
    }

    inline FakeObject *object() const
    {
        return m_object;
    }

Q_SIGNALS:
    void aboutToBeInvalidated();

private:
    FakeObject *m_object;
};
