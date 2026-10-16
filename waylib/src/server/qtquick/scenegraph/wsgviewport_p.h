// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <typeinfo>
#include <utility>
#include <QList>
#include <wglobal.h>
#include <wpixmanregion.h>

#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
WAYLIB_SERVER_BEGIN_NAMESPACE

// Per-render-target mapping plus that target's accumulated scene damage.
// WSGDamageTracker::commit(WSGViewport&) copies this round's scene damage
// into every viewport; each viewport keeps it until it presents, so outputs
// sharing one scene at different refresh rates never lose damage.
// Mapping changes (matrix / source / target) mark full: the whole
// buffer contents are invalid. Size and DPR are WBufferRenderer's job.
// Damage stays in scene coordinates; whether it hits this output is decided
// with the current mapping and the buffer pixel size at draw time.
class WAYLIB_SERVER_EXPORT WSGViewport
{
public:
    WSGViewport();
    ~WSGViewport();

    template<typename T>
    T *getAttachedData(const void *owner) const {
        int index = indexOfAttachedData(owner);
        if (index < 0)
            return nullptr;
        return reinterpret_cast<T*>(m_attachedDatas.at(index).data);
    }

    template<typename T>
    T *getAttachedData() const {
        const void *owner = typeid(T).name();
        return getAttachedData<T>(owner);
    }

    template<typename T>
    void setAttachedData(const void *owner, void *data, void (*deleter)(void*) = nullptr) {
        int index = indexOfAttachedData(owner);
        if (index >= 0) {
            if (m_attachedDatas[index].deleter && m_attachedDatas[index].data != data)
                m_attachedDatas[index].deleter(m_attachedDatas[index].data);
            m_attachedDatas[index].data = data;
            m_attachedDatas[index].deleter = deleter;
        } else {
            m_attachedDatas.append({owner, data, deleter});
        }
    }

    template<typename T>
    void setAttachedData(void *data, void (*deleter)(void*) = nullptr) {
        const void *owner = typeid(T).name();
        setAttachedData<T>(owner, data, deleter);
    }

    template<typename T>
    void removeAttachedData(const void *owner) {
        int index = indexOfAttachedData(owner);
        if (index >= 0) {
            if (m_attachedDatas[index].deleter)
                m_attachedDatas[index].deleter(m_attachedDatas[index].data);
            m_attachedDatas.removeAt(index);
        }
    }

    template<typename T>
    void removeAttachedData() {
        const void *owner = typeid(T).name();
        removeAttachedData<T>(owner);
    }

    int indexOfAttachedData(const void *owner) const;

    void setRenderParameters(const QMatrix4x4 &renderMatrix,
                             const QRectF &sourceRect = {},
                             const QRectF &targetRect = {});

    const QMatrix4x4 &renderMatrix() const;
    QRectF sourceRect() const;
    QRectF targetRect() const;

    // True when accumulated scene damage intersects this viewport's source
    // region. Unbounded or unmappable viewports conservatively report dirty.
    bool isDirty() const;


    const WDamageRegion &damageRegion() const
    {
        return m_damageRegion;
    }

    bool isFull() const
    {
        return m_damageRegion.isFull;
    }

    void markFull();
    void addDamage(const WDamageRegion &damage);
    void finishFrame();

private:
    QMatrix4x4 m_renderMatrix;
    QRectF m_sourceRect;
    QRectF m_targetRect;
    WDamageRegion m_damageRegion;

    struct AttachedEntry {
        const void *owner = nullptr;
        void *data = nullptr;
        void (*deleter)(void*) = nullptr;
    };
    QList<AttachedEntry> m_attachedDatas;
};

WAYLIB_SERVER_END_NAMESPACE
