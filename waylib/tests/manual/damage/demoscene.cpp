// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "demoscene.h"
#include <QDateTime>

#include <cmath>

WAYLIB_SERVER_USE_NAMESPACE

static const QColor kPalette[] = {
    QColor("#5b8def"),
    QColor("#9353d3"),
    QColor("#f5a524"),
    QColor("#00bcd4"),
    QColor("#6366f1"),
    QColor("#0284c7"),
    QColor("#8b5cf6"),
    QColor("#d97706"),
};

static QString typeString(WSGDamageNode::Type t)
{
    switch (t) {
    case WSGDamageNode::Type::Basic:
        return QStringLiteral("Basic");
    case WSGDamageNode::Type::Transform:
        return QStringLiteral("Transform");
    case WSGDamageNode::Type::Clip:
        return QStringLiteral("Clip");
    case WSGDamageNode::Type::Geometry:
        return QStringLiteral("Geometry");
    }
    return {};
}

static QVariantMap clipVisualRow(WSGDamageNode *n, const QString &name, int paintOrder)
{
    auto *clip = n->toClip();
    const QRectF r = clip->clipRect();
    const QTransform matrix = n->worldTransform();
    const QRect aabb = mapOuter(matrix, r);
    QVariantMap v;
    v.insert(QStringLiteral("id"), n->id());
    v.insert(QStringLiteral("name"), name);
    v.insert(QStringLiteral("type"), QStringLiteral("Clip"));
    v.insert(QStringLiteral("localX"), r.x());
    v.insert(QStringLiteral("localY"), r.y());
    v.insert(QStringLiteral("localWidth"), r.width());
    v.insert(QStringLiteral("localHeight"), r.height());
    v.insert(QStringLiteral("m11"), matrix.m11());
    v.insert(QStringLiteral("m12"), matrix.m12());
    v.insert(QStringLiteral("m21"), matrix.m21());
    v.insert(QStringLiteral("m22"), matrix.m22());
    v.insert(QStringLiteral("m13"), matrix.m13());
    v.insert(QStringLiteral("m23"), matrix.m23());
    v.insert(QStringLiteral("m33"), matrix.m33());
    v.insert(QStringLiteral("dx"), matrix.dx());
    v.insert(QStringLiteral("dy"), matrix.dy());
    v.insert(QStringLiteral("x"), aabb.x());
    v.insert(QStringLiteral("y"), aabb.y());
    v.insert(QStringLiteral("w"), aabb.width());
    v.insert(QStringLiteral("h"), aabb.height());
    v.insert(QStringLiteral("hasContent"), false);
    v.insert(QStringLiteral("occluded"), false);
    v.insert(QStringLiteral("culled"), false);
    v.insert(QStringLiteral("visible"), n->isVisible());
    v.insert(QStringLiteral("color"), QStringLiteral("#22d3ee"));
    v.insert(QStringLiteral("isBackdrop"), false);
    v.insert(QStringLiteral("fullyOpaque"), false);
    v.insert(QStringLiteral("paintOrder"), paintOrder);
    return v;
}

static bool worldHidden(const WSGDamageNode *n)
{
    return WPixmanRegion(n->worldValidRegion()).isEmpty();
}

static int refreshInterval(int refreshRate)
{
    return qMax(1, int(std::round(1000.0 / refreshRate)));
}
static QVariantList regionToRects(const QRegion &region)
{
    QVariantList list;
    for (const QRect &r : region) {
        QVariantMap m;
        m.insert(QStringLiteral("x"), r.x());
        m.insert(QStringLiteral("y"), r.y());
        m.insert(QStringLiteral("w"), r.width());
        m.insert(QStringLiteral("h"), r.height());
        list.append(m);
    }
    return list;
}

DemoScene::DemoScene(QObject *parent)
    : QObject(parent)
    , m_visualNodeModel(new VisualNodeModel(this))
{
    m_dragFrameTimer.setInterval(refreshInterval(m_refreshRate));
    connect(&m_dragFrameTimer, &QTimer::timeout, this, [this] {
        if (!m_dragFramePending) {
            m_dragFrameTimer.stop();
            return;
        }
        m_dragFramePending = false;
        updateDamage(false);
    });
    m_demoTimer.setInterval(refreshInterval(m_refreshRate));
    connect(&m_demoTimer, &QTimer::timeout, this, &DemoScene::advanceDemoFrame);
    resetRoot();
    loadPreset(QStringLiteral("occlusion"));
}

DemoScene::~DemoScene() = default;

void DemoScene::resetRoot()
{
    const bool selectionChanged = m_selectedId != 0;
    m_root = std::make_unique<WSGDamageNode>();
    m_root->setName(QStringLiteral("根节点"));
    m_tracker.setRoot(m_root.get());
    m_decor.clear();
    m_dragFrameTimer.stop();
    m_dragFramePending = false;
    m_damageFrames.clear();
    m_selectedId = 0;
    m_colorIndex = 0;
    if (selectionChanged)
        emit selectedIdChanged();
}

void DemoScene::setSelectedId(quint64 id)
{
    if (m_selectedId == id)
        return;
    m_selectedId = id;

    // Decompose transform state from matrix (best effort).
    if (WSGDamageNode *n = findNode(id)) {
        if (auto *tr = n->toTransform()) {
            const QTransform m = tr->matrix();
            if (!qFuzzyIsNull(m.m13()) || !qFuzzyIsNull(m.m23())) {
                // Non-affine: perspective transform from X or Y axis rotation.
                if (!qFuzzyIsNull(m.m23()))
                    m_rotationAxis = 0;  // X axis: m23 = -sin/d
                else
                    m_rotationAxis = 1;  // Y axis: m13 = sin/d
                m_rotation = 0;
                m_scaleX = 1;
                m_scaleY = 1;
            } else {
                // Affine (Z axis): standard decomposition.
                m_rotationAxis = 2;
                m_rotation = std::atan2(m.m12(), m.m11())
                    * 180.0 / 3.14159265358979323846;
                m_scaleX = std::hypot(m.m11(), m.m12());
                m_scaleY = std::hypot(m.m21(), m.m22());
            }
        } else {
            m_rotationAxis = 2;
            m_rotation = 0;
            m_scaleX = 1;
            m_scaleY = 1;
        }
    }
    emit selectedIdChanged();
    rebuildLists();
    refreshSelectedProps();
}

void DemoScene::setAutoCommit(bool enabled)
{
    if (m_autoCommit == enabled)
        return;
    m_autoCommit = enabled;
    emit autoCommitChanged();
}

void DemoScene::setRefreshRate(int refreshRate)
{
    refreshRate = qBound(1, refreshRate, 1000);
    if (m_refreshRate == refreshRate)
        return;
    m_refreshRate = refreshRate;
    m_demoTimer.setInterval(refreshInterval(m_refreshRate));
    if (m_demoRunning)
        m_demoTimer.start();
    m_dragFrameTimer.setInterval(refreshInterval(m_refreshRate));
    emit refreshRateChanged();
}

QVariantList DemoScene::demoScenes() const
{
    return {
        QVariantMap{{QStringLiteral("text"), QStringLiteral("遮挡移动")},
                    {QStringLiteral("value"), QStringLiteral("occlusion")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("局部内容变化")},
                    {QStringLiteral("value"), QStringLiteral("content")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("裁剪视口滚动")},
                    {QStringLiteral("value"), QStringLiteral("clip")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("背景采样扩散")},
                    {QStringLiteral("value"), QStringLiteral("backdrop")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("揭露后方节点")},
                    {QStringLiteral("value"), QStringLiteral("reveal")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("自定义渲染节点")},
                    {QStringLiteral("value"), QStringLiteral("renderer")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("旋转节点")},
                    {QStringLiteral("value"), QStringLiteral("rotation")}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("缩放节点")},
                    {QStringLiteral("value"), QStringLiteral("scale")}},
    };
}

void DemoScene::setDemoRunning(bool running)
{
    if (m_demoRunning == running)
        return;
    m_demoRunning = running;
    if (m_demoRunning)
        m_demoTimer.start();
    else
        m_demoTimer.stop();
    emit demoRunningChanged();
}


void DemoScene::loadDemoScene(const QString &name)
{
    const QString sceneName = name.isEmpty() ? QStringLiteral("occlusion") : name;
    m_demoTimer.stop();
    m_demoFrame = 0;
    m_demoNodeA = 0;
    m_demoNodeB = 0;
    m_demoNodeC = 0;
    m_demoRunning = true;
    m_demoSceneName = sceneName;
    resetRoot();
    buildDemoScene(sceneName);
    commit();
    m_demoTimer.setInterval(refreshInterval(m_refreshRate));
    m_demoTimer.start();
    emit demoRunningChanged();
    emit demoSceneChanged();
}

void DemoScene::stepDemoFrame()
{
    advanceDemoFrame();
}
void DemoScene::activateNode(quint64 id)
{
    WSGDamageNode *node = findNode(id);
    if (!node)
        return;
    setSelectedId(id);
    if (!node->parent() || !node->nextSibling())
        return;
    node->parent()->appendChild(node);
    maybeCommit();
}

WSGDamageNode *DemoScene::findNodeRecursive(WSGDamageNode *n, quint64 id) const
{
    if (!n)
        return nullptr;
    if (n->id() == id)
        return n;
    for (WSGDamageNode *c = n->firstChild(); c; c = c->nextSibling()) {
        if (WSGDamageNode *f = findNodeRecursive(c, id))
            return f;
    }
    return nullptr;
}

WSGDamageNode *DemoScene::findNode(quint64 id) const
{
    return findNodeRecursive(m_root.get(), id);
}

WSGDamageNode *DemoScene::parentForInsert() const
{
    if (WSGDamageNode *sel = findNode(m_selectedId))
        return sel;
    return m_root.get();
}

bool DemoScene::isDescendantOf(const WSGDamageNode *node, const WSGDamageNode *ancestor) const
{
    for (const WSGDamageNode *current = node; current; current = current->parent()) {
        if (current == ancestor)
            return true;
    }
    return false;
}

void DemoScene::moveNode(quint64 nodeId, quint64 newParentId, quint64 beforeSiblingId)
{

    WSGDamageNode *node = findNode(nodeId);
    WSGDamageNode *newParent = newParentId ? findNode(newParentId) : m_root.get();
    WSGDamageNode *before = beforeSiblingId ? findNode(beforeSiblingId) : nullptr;
    if (!node || !newParent || node == m_root.get() || node == newParent
        || isDescendantOf(newParent, node))
        return;
    if (before && (before == node || before->parent() != newParent))
        return;
    if (before && before->parent() == node)
        return;

    if (node->parent() == newParent && !before && node == newParent->lastChild())
        return;
    if (node->parent() == newParent && before == node->nextSibling())
        return;

    if (before)
        newParent->insertChildBefore(node, before);
    else
        newParent->appendChild(node);
    setSelectedId(nodeId);
    maybeCommit();
}

QColor DemoScene::nextColor()
{
    const QColor c = kPalette[m_colorIndex % int(sizeof(kPalette) / sizeof(kPalette[0]))];
    ++m_colorIndex;
    return c;
}

void DemoScene::addBasic()
{
    auto *n = new WSGDamageNode;
    n->setName(QStringLiteral("分组"));
    parentForInsert()->appendChild(n);
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::addTransform()
{
    auto *n = new WSGDamageTransformNode;
    n->setName(QStringLiteral("变换"));
    parentForInsert()->appendChild(n);
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::addClip()
{
    auto *n = new WSGDamageClipNode;
    n->setName(QStringLiteral("裁剪"));
    n->setClipRect(QRectF(40, 40, 280, 200));
    parentForInsert()->appendChild(n);
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::addGeometry()
{
    auto *n = new WSGDamageGeometryNode;
    n->setName(QStringLiteral("矩形"));
    n->setBoundingRect(QRectF(80 + m_colorIndex * 16, 80 + m_colorIndex * 12, 140, 90));
    n->setFullyOpaque(true);
    parentForInsert()->appendChild(n);
    m_decor.insert(n->id(), Decor{nextColor()});
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::addBackdrop()
{
    auto *n = new WSGDamageBackdropNode;
    n->setName(QStringLiteral("背景采样"));
    n->setBoundingRect(QRectF(40, 40, 280, 200));
    parentForInsert()->appendChild(n);
    m_decor.insert(n->id(), Decor{QColor("#00bcd4")});
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::addRenderer()
{
    auto *n = new WSGDamageGeometryNode;
    n->setName(QStringLiteral("自定义渲染"));
    n->setBoundingRect(QRectF(60 + m_colorIndex * 16, 60 + m_colorIndex * 12, 180, 120));
    parentForInsert()->appendChild(n);
    m_decor.insert(n->id(), Decor{QColor("#ff7043")});
    setSelectedId(n->id());
    maybeCommit();
}

void DemoScene::removeSelected()
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || n == m_root.get())
        return;
    const quint64 parentId = n->parent() ? n->parent()->id() : 0;
    delete n;
    m_decor.remove(m_selectedId);
    setSelectedId(parentId);
    maybeCommit();
}

void DemoScene::raiseSelected()
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->parent() || !n->nextSibling())
        return;
    WSGDamageNode *parent = n->parent();
    WSGDamageNode *after = n->nextSibling();
    parent->insertChildAfter(n, after);
    maybeCommit();
}

void DemoScene::lowerSelected()
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->parent() || !n->previousSibling())
        return;
    WSGDamageNode *parent = n->parent();
    WSGDamageNode *before = n->previousSibling();
    parent->insertChildBefore(n, before);
    maybeCommit();
}

void DemoScene::setVisibleSelected(bool visible)
{
    if (WSGDamageNode *n = findNode(m_selectedId)) {
        n->setVisible(visible);
        maybeCommit();
    }
}


void DemoScene::setRectSelected(qreal x, qreal y, qreal w, qreal h)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n)
        return;
    if (auto *geo = n->toGeometry())
        geo->setBoundingRect(QRectF(x, y, w, h));
    else if (auto *clip = n->toClip())
        clip->setClipRect(QRectF(x, y, w, h));
    else
        return;
    maybeCommit();
}

void DemoScene::setTranslationSelected(qreal x, qreal y)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toTransform())
        return;
    QTransform matrix = n->toTransform()->matrix();
    matrix.setMatrix(matrix.m11(), matrix.m12(), matrix.m13(),
                     matrix.m21(), matrix.m22(), matrix.m23(),
                     x, y, matrix.m33());
    n->toTransform()->setMatrix(matrix);
    maybeCommit();
}

void DemoScene::setRotationSelected(qreal degrees, int axis)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toTransform())
        return;
    m_rotationAxis = axis;
    m_rotation = degrees;
    const QTransform current = n->toTransform()->matrix();
    QTransform matrix;
    matrix.translate(current.dx(), current.dy());
    const Qt::Axis a = (axis == 0) ? Qt::XAxis : ((axis == 1) ? Qt::YAxis : Qt::ZAxis);
    matrix.rotate(degrees, a);
    matrix.scale(m_scaleX, m_scaleY);
    n->toTransform()->setMatrix(matrix);
    maybeCommit();
}

void DemoScene::setScaleSelected(qreal sx, qreal sy)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toTransform())
        return;
    m_scaleX = sx;
    m_scaleY = sy;
    const QTransform current = n->toTransform()->matrix();
    QTransform matrix;
    matrix.translate(current.dx(), current.dy());
    const Qt::Axis a = (m_rotationAxis == 0) ? Qt::XAxis : ((m_rotationAxis == 1) ? Qt::YAxis : Qt::ZAxis);
    matrix.rotate(m_rotation, a);
    matrix.scale(sx, sy);
    n->toTransform()->setMatrix(matrix);
    maybeCommit();
}

void DemoScene::setFullyOpaqueSelected(bool opaque)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toGeometry())
        return;
    n->toGeometry()->setFullyOpaque(opaque);
    maybeCommit();
}

void DemoScene::setExpansionSelected(int px)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->needsBackdrop())
        return;
    Decor &decor = m_decor[n->id()];
    const int clamped = qMax(0, px);
    if (decor.expansion == clamped)
        return;
    decor.expansion = clamped;
    if (auto *backdrop = n->toBackdrop())
        backdrop->setRecopyExpansion(QMargins(clamped, clamped, clamped, clamped));
    maybeCommit();
}

void DemoScene::setClipExpansionSelected(bool clip)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->needsBackdrop())
        return;
    Decor &decor = m_decor[n->id()];
    if (decor.clipExpansion == clip)
        return;
    decor.clipExpansion = clip;
    maybeCommit();
}

void DemoScene::markSelectedContentDirty()
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toGeometry())
        return;
    const QRectF br = n->toGeometry()->boundingRect();
    n->toGeometry()->markContentDirty(QRect(int(br.x()), int(br.y()),
                                            int(br.width()), int(br.height())));
    maybeCommit();
}

void DemoScene::markSelectedContentDirtyAt(qreal x, qreal y, qreal w, qreal h)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n || !n->toGeometry())
        return;
    n->toGeometry()->markContentDirty(QRect(int(std::floor(x)), int(std::floor(y)),
                                            int(std::ceil(w)), int(std::ceil(h))));
    maybeCommit();
}

void DemoScene::moveSelectedBy(qreal dx, qreal dy)
{
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n)
        return;
    if (auto *tr = n->toTransform()) {
        const QTransform m = tr->matrix();
        tr->setTranslation(m.dx() + dx, m.dy() + dy);
    } else if (auto *geo = n->toGeometry()) {
        QRectF r = geo->boundingRect();
        r.translate(dx, dy);
        geo->setBoundingRect(r);
    } else if (auto *clip = n->toClip()) {
        QRectF r = clip->clipRect();
        r.translate(dx, dy);
        clip->setClipRect(r);
    } else {
        return;
    }

    m_dragFramePending = true;
    if (!m_dragFrameTimer.isActive()) {
        m_dragFramePending = false;
        updateDamage(false);
        m_dragFrameTimer.start();
    }
}

void DemoScene::finishSelectedMove()
{
    if (m_dragFramePending)
        updateDamage(false);
    m_dragFramePending = false;
    m_dragFrameTimer.stop();
    rebuildLists();
    refreshSelectedProps();
}

void DemoScene::clearTree()
{
    resetRoot();
    m_damageRects.clear();
    m_damageRectsB.clear();
    m_damageFrames.clear();
    rebuildLists();
    refreshSelectedProps();
    emit damageChanged();
}

void DemoScene::loadPreset(const QString &name)
{
    resetRoot();

    if (name == QLatin1String("occlusion")) {
        auto *back = new WSGDamageGeometryNode;
        back->setName(QStringLiteral("后层不透明节点"));
        back->setBoundingRect(QRectF(80, 80, 260, 200));
        back->setFullyOpaque(true);
        m_decor.insert(back->id(), Decor{QColor("#5b8def")});

        auto *front = new WSGDamageGeometryNode;
        front->setName(QStringLiteral("前层不透明节点"));
        front->setBoundingRect(QRectF(160, 140, 220, 180));
        front->setFullyOpaque(true);
        m_decor.insert(front->id(), Decor{QColor("#9353d3")});

        m_root->appendChild(back);
        m_root->appendChild(front);
        setSelectedId(back->id());
    } else if (name == QLatin1String("backdrop")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("壁纸"));
        wall->setBoundingRect(QRectF(40, 40, 520, 360));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#0284c7")});

        auto *bg = new WSGDamageBackdropNode;
        bg->setName(QStringLiteral("模糊层"));
        bg->setBoundingRect(QRectF(140, 100, 280, 200));
            m_decor.insert(bg->id(), Decor{QColor("#00bcd4")});

        auto *chip = new WSGDamageGeometryNode;
        chip->setName(QStringLiteral("前景卡片"));
        chip->setBoundingRect(QRectF(200, 160, 80, 60));
        chip->setFullyOpaque(true);
        m_decor.insert(chip->id(), Decor{QColor("#f5a524")});

        m_root->appendChild(wall);
        m_root->appendChild(bg);
        m_root->appendChild(chip);
        setSelectedId(wall->id());
    } else if (name == QLatin1String("transform")) {
        auto *tr = new WSGDamageTransformNode;
        tr->setName(QStringLiteral("位移组"));
        tr->setTranslation(40, 30);

        auto *g = new WSGDamageGeometryNode;
        g->setName(QStringLiteral("卡片"));
        g->setBoundingRect(QRectF(80, 80, 160, 120));
        g->setFullyOpaque(true);
        m_decor.insert(g->id(), Decor{QColor("#9353d3")});

        auto *g2 = new WSGDamageGeometryNode;
        g2->setName(QStringLiteral("徽标"));
        g2->setBoundingRect(QRectF(200, 140, 90, 70));
        g2->setFullyOpaque(true);
        m_decor.insert(g2->id(), Decor{QColor("#6366f1")});

        tr->appendChild(g);
        tr->appendChild(g2);
        m_root->appendChild(tr);
        setSelectedId(tr->id());
    } else {
        addGeometry();
        return;
    }

    commit();
}

void DemoScene::buildDemoScene(const QString &name)
{
    if (name == QLatin1String("content")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("内容背景"));
        wall->setBoundingRect(QRectF(20, 20, 560, 380));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#0284c7")});

        auto *content = new WSGDamageGeometryNode;
        content->setName(QStringLiteral("局部变化内容"));
        content->setBoundingRect(QRectF(100, 110, 240, 150));
        m_decor.insert(content->id(), Decor{QColor("#f5a524")});
        m_root->appendChild(wall);
        m_root->appendChild(content);
        m_demoNodeA = content->id();
        setSelectedId(content->id());
        return;
    }

    if (name == QLatin1String("clip")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("裁剪外背景"));
        wall->setBoundingRect(QRectF(20, 20, 320, 440));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#0284c7")});

        auto *clip = new WSGDamageClipNode;
        clip->setName(QStringLiteral("视口裁剪"));
        clip->setClipRect(QRectF(40, 50, 280, 300));

        auto *scroll = new WSGDamageTransformNode;
        scroll->setName(QStringLiteral("列表滚动"));

        const QColor cardColors[] = {
            QColor("#5b8def"), QColor("#9353d3"), QColor("#f5a524"),
            QColor("#00bcd4"), QColor("#6366f1"),
        };
        for (int i = 0; i < 5; ++i) {
            auto *card = new WSGDamageGeometryNode;
            card->setName(QStringLiteral("列表卡片"));
            card->setBoundingRect(QRectF(50, 60 + i * 100, 260, 80));
            card->setFullyOpaque(true);
            m_decor.insert(card->id(), Decor{cardColors[i]});
            scroll->appendChild(card);
        }
        clip->appendChild(scroll);
        m_root->appendChild(wall);
        m_root->appendChild(clip);
        m_demoNodeA = scroll->id();
        m_demoNodeB = clip->id();
        setSelectedId(clip->id());
        return;
    }

    if (name == QLatin1String("backdrop")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("被采样背景"));
        wall->setBoundingRect(QRectF(20, 20, 560, 380));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#0284c7")});

        auto *backdrop = new WSGDamageBackdropNode;
        backdrop->setName(QStringLiteral("背景采样"));
        backdrop->setBoundingRect(QRectF(150, 100, 280, 200));
            m_decor.insert(backdrop->id(), Decor{QColor("#00bcd4")});

        auto *chip = new WSGDamageGeometryNode;
        chip->setName(QStringLiteral("前景内容"));
        chip->setBoundingRect(QRectF(230, 165, 90, 70));
        chip->setFullyOpaque(true);
        m_decor.insert(chip->id(), Decor{QColor("#f5a524")});
        m_root->appendChild(wall);
        m_root->appendChild(backdrop);
        m_root->appendChild(chip);
        m_demoNodeA = wall->id();
        m_demoNodeB = backdrop->id();
        m_demoNodeC = chip->id();
        setSelectedId(backdrop->id());
        return;
    }

    if (name == QLatin1String("rotation")) {
        auto *transform = new WSGDamageTransformNode;
        transform->setName(QStringLiteral("旋转变换"));
        QTransform matrix;
        matrix.translate(300, 220);
        matrix.rotate(0, Qt::ZAxis);
        transform->setMatrix(matrix);

        auto *geometry = new WSGDamageGeometryNode;
        geometry->setName(QStringLiteral("旋转卡片"));
        geometry->setBoundingRect(QRectF(-110, -70, 220, 140));
        geometry->setFullyOpaque(true);
        m_decor.insert(geometry->id(), Decor{QColor("#9353d3")});
        transform->appendChild(geometry);
        m_root->appendChild(transform);
        m_demoNodeA = transform->id();
        m_demoNodeB = geometry->id();
        setSelectedId(transform->id());
        return;
    }

    if (name == QLatin1String("scale")) {
        auto *transform = new WSGDamageTransformNode;
        transform->setName(QStringLiteral("缩放变换"));
        QTransform matrix;
        matrix.translate(180, 150);
        transform->setMatrix(matrix);

        auto *geometry = new WSGDamageGeometryNode;
        geometry->setName(QStringLiteral("缩放卡片"));
        geometry->setBoundingRect(QRectF(0, 0, 220, 140));
        geometry->setFullyOpaque(true);
        m_decor.insert(geometry->id(), Decor{QColor("#0284c7")});
        transform->appendChild(geometry);
        m_root->appendChild(transform);
        m_demoNodeA = transform->id();
        m_demoNodeB = geometry->id();
        setSelectedId(transform->id());
        return;
    }

    if (name == QLatin1String("reveal")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("后方静止底板"));
        wall->setBoundingRect(QRectF(50, 60, 480, 280));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#5b8def")});

        auto *badge = new WSGDamageGeometryNode;
        badge->setName(QStringLiteral("被遮挡内容"));
        badge->setBoundingRect(QRectF(170, 100, 200, 160));
        badge->setFullyOpaque(true);
        m_decor.insert(badge->id(), Decor{QColor("#00bcd4")});

        auto *cover = new WSGDamageGeometryNode;
        cover->setName(QStringLiteral("移开的遮挡窗口"));
        cover->setBoundingRect(QRectF(150, 80, 240, 200));
        cover->setFullyOpaque(true);
        m_decor.insert(cover->id(), Decor{QColor("#9353d3")});

        m_root->appendChild(wall);
        m_root->appendChild(badge);
        m_root->appendChild(cover);
        m_demoNodeA = wall->id();
        m_demoNodeB = cover->id();
        setSelectedId(cover->id());
        return;
    }

    if (name == QLatin1String("renderer")) {
        auto *wall = new WSGDamageGeometryNode;
        wall->setName(QStringLiteral("底层动态卡片"));
        wall->setBoundingRect(QRectF(60, 60, 240, 180));
        wall->setFullyOpaque(true);
        m_decor.insert(wall->id(), Decor{QColor("#5b8def")});

        auto *rnd = new WSGDamageGeometryNode;
        rnd->setName(QStringLiteral("自适应渲染器"));
        rnd->setBoundingRect(QRectF(140, 100, 240, 160));
        m_decor.insert(rnd->id(), Decor{QColor("#ff7043")});

        m_root->appendChild(wall);
        m_root->appendChild(rnd);
        m_demoNodeA = wall->id();
        m_demoNodeB = rnd->id();
        setSelectedId(rnd->id());
        return;
    }

    auto *back = new WSGDamageGeometryNode;
    back->setName(QStringLiteral("被遮挡内容"));
    back->setBoundingRect(QRectF(80, 80, 260, 200));
    back->setFullyOpaque(true);
    m_decor.insert(back->id(), Decor{QColor("#5b8def")});
        auto *front = new WSGDamageGeometryNode;
        front->setName(QStringLiteral("移动遮挡层"));
        front->setBoundingRect(QRectF(160, 140, 220, 180));
        front->setFullyOpaque(true);
        m_decor.insert(front->id(), Decor{QColor("#9353d3")});
        m_root->appendChild(back);
    m_root->appendChild(front);
    m_demoNodeA = back->id();
    m_demoNodeB = front->id();
    setSelectedId(front->id());
}

void DemoScene::advanceDemoFrame()
{
    if (!m_demoRunning)
        return;

    const int phase = ++m_demoFrame % 120;
    const int triangle = phase < 60 ? phase : 120 - phase;
    if (m_demoSceneName == QLatin1String("content")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *geometry = node->toGeometry())
                geometry->markContentDirty(QRect(112 + triangle * 2, 134, 26, 20));
        }
    } else if (m_demoSceneName == QLatin1String("clip")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *transform = node->toTransform())
                transform->setTranslation(0, -triangle * 3.2);
        }
    } else if (m_demoSceneName == QLatin1String("backdrop")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *geometry = node->toGeometry())
                geometry->markContentDirty(QRect(36 + triangle, 46, 24, 18));
        }
    } else if (m_demoSceneName == QLatin1String("renderer")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *geometry = node->toGeometry())
                geometry->setBoundingRect(QRectF(60 + triangle * 2, 60, 240, 180));
        }
    } else if (m_demoSceneName == QLatin1String("rotation")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *transform = node->toTransform()) {
                QTransform matrix;
                matrix.translate(300, 220);
                const Qt::Axis a = (m_rotationAxis == 0) ? Qt::XAxis : ((m_rotationAxis == 1) ? Qt::YAxis : Qt::ZAxis);
                matrix.rotate(triangle * 3.0, a);
                transform->setMatrix(matrix);
            }
        }
    } else if (m_demoSceneName == QLatin1String("scale")) {
        if (auto *node = findNode(m_demoNodeA)) {
            if (auto *transform = node->toTransform()) {
                const qreal scale = 0.65 + qreal(triangle) / 60.0 * 0.75;
                QTransform matrix;
                matrix.translate(180, 150);
                matrix.scale(scale, scale);
                transform->setMatrix(matrix);
            }
        }
    } else if (m_demoSceneName == QLatin1String("reveal")) {
        if (auto *node = findNode(m_demoNodeB)) {
            if (auto *geometry = node->toGeometry())
                geometry->setBoundingRect(QRectF(150 + triangle * 3.5, 80, 240, 200));
        }
    } else {
        if (auto *node = findNode(m_demoNodeB)) {
            if (auto *geometry = node->toGeometry())
                geometry->setBoundingRect(QRectF(80 + triangle * 3, 140, 220, 180));
        }
    }
    updateDamage(false);
}

void DemoScene::commit()
{
    updateDamage(true);
}

void DemoScene::updateDamage(bool rebuildScene)
{
    m_commitViewports = {
        WSGViewport(m_viewportA),
        WSGViewport(m_viewportB),
    };
    m_tracker.prepareFrame();
    m_tracker.commit(m_commitViewports[0]);
    m_tracker.commit(m_commitViewports[1]);
    m_tracker.accumulateFrame({ });
    m_tracker.finishFrame();
    const QRegion flush = m_tracker.frameFlush().toQRegion();
    QRegion damageA = WPixmanRegion(m_commitViewports[0].outputDamageRegion()).toQRegion() + flush;
    QRegion damageB = WPixmanRegion(m_commitViewports[1].outputDamageRegion()).toQRegion() + flush;
    if (!m_viewportA.isEmpty())
        damageA &= m_viewportA;
    if (!m_viewportB.isEmpty())
        damageB &= m_viewportB;
    m_damageRects = regionToRects(damageA);
    m_damageRectsB = regionToRects(damageB);

    const QRegion combined = damageA + damageB;
    if (!combined.isEmpty()) {
        QVariantMap frame;
        frame.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        frame.insert(QStringLiteral("rects"), regionToRects(combined));
        m_damageFrames.append(frame);
        constexpr qsizetype maximumFrames = 120;
        while (m_damageFrames.size() > maximumFrames)
            m_damageFrames.removeFirst();
    }

    if (rebuildScene)
        rebuildLists();
    else
        rebuildVisualNodes();
    refreshSelectedProps();
    emit damageChanged();
}

void DemoScene::maybeCommit()
{
    if (m_autoCommit)
        commit();
    else {
        rebuildLists();
        refreshSelectedProps();
    }
}

void DemoScene::rebuildLists()
{
    QVector<QVariantMap> visual;
    QVariantList tree;
    int paintOrder = 0;
    m_displayNames.clear();
    collectVisual(m_root.get(), &visual, &tree, 0, &paintOrder);
    m_visualNodeModel->setNodes(visual);
    m_treeNodes = tree;
    emit treeNodesChanged();
    emit sceneChanged();
}

void DemoScene::rebuildVisualNodes()
{
    QVector<QVariantMap> visual;
    int paintOrder = 0;
    collectVisualOnly(m_root.get(), &visual, &paintOrder);
    m_visualNodeModel->setNodes(visual);
    emit sceneChanged();
}

void DemoScene::collectVisualOnly(WSGDamageNode *n, QVector<QVariantMap> *visual, int *paintOrder)
{
    if (n->hasContent()) {
        auto *geo = n->toGeometry();
        const QRectF localRect = geo->boundingRect();
        const QTransform matrix = n->worldTransform();
        const QRect aabb = n->worldBounds();

        QVariantMap v;
        v.insert(QStringLiteral("id"), n->id());
        v.insert(QStringLiteral("name"), m_displayNames.value(n->id(), n->name()));
        v.insert(QStringLiteral("type"), typeString(n->type()));
        v.insert(QStringLiteral("localX"), localRect.x());
        v.insert(QStringLiteral("localY"), localRect.y());
        v.insert(QStringLiteral("localWidth"), localRect.width());
        v.insert(QStringLiteral("localHeight"), localRect.height());
        v.insert(QStringLiteral("m11"), matrix.m11());
        v.insert(QStringLiteral("m12"), matrix.m12());
        v.insert(QStringLiteral("m21"), matrix.m21());
        v.insert(QStringLiteral("m22"), matrix.m22());
        v.insert(QStringLiteral("m13"), matrix.m13());
        v.insert(QStringLiteral("m23"), matrix.m23());
        v.insert(QStringLiteral("m33"), matrix.m33());
        v.insert(QStringLiteral("dx"), matrix.dx());
        v.insert(QStringLiteral("dy"), matrix.dy());
        v.insert(QStringLiteral("x"), aabb.x());
        v.insert(QStringLiteral("y"), aabb.y());
        v.insert(QStringLiteral("w"), aabb.width());
        v.insert(QStringLiteral("h"), aabb.height());
        v.insert(QStringLiteral("hasContent"), n->hasContent());
        v.insert(QStringLiteral("occluded"), worldHidden(n));
        v.insert(QStringLiteral("culled"), worldHidden(n));
        v.insert(QStringLiteral("visible"), n->isVisible());
        const QColor c = m_decor.value(n->id()).color;
        v.insert(QStringLiteral("color"), (c.isValid() ? c : QColor("#888888")).name());
        v.insert(QStringLiteral("isBackdrop"), n->needsBackdrop());
        v.insert(QStringLiteral("fullyOpaque"), geo->isFullyOpaque());
        v.insert(QStringLiteral("paintOrder"), (*paintOrder)++);
        visual->append(v);
    } else if (n->toClip()) {
        visual->append(clipVisualRow(n, m_displayNames.value(n->id(), n->name()), (*paintOrder)++));
    }
    for (WSGDamageNode *c = n->firstChild(); c; c = c->nextSibling())
        collectVisualOnly(c, visual, paintOrder);
}

void DemoScene::collectVisual(WSGDamageNode *n, QVector<QVariantMap> *visual, QVariantList *tree,
                              int depth, int *paintOrder)
{
    const int order = (*paintOrder)++;
    QString label;
    if (!n->parent()) {
        label = QStringLiteral("根节点 #%1").arg(n->id());
    } else if (n->needsBackdrop()) {
        auto *bg = n->toGeometry();
        const QRectF r = bg->boundingRect();
        label = QStringLiteral("背景采样 #%1 [%2x%3]")
                    .arg(n->id())
                    .arg(qRound(r.width()))
                    .arg(qRound(r.height()));
    } else if (auto *geometry = n->toGeometry()) {
        const QRectF r = geometry->boundingRect();
        const QString typeDesc = geometry->isFullyOpaque() ? QStringLiteral("不透明几何")
                                                           : QStringLiteral("透明几何");
        label = QStringLiteral("%1 #%2 [%3x%4]")
                    .arg(typeDesc)
                    .arg(n->id())
                    .arg(qRound(r.width()))
                    .arg(qRound(r.height()));
    } else if (auto *clip = n->toClip()) {
        const QRectF r = clip->clipRect();
        label = QStringLiteral("裁剪节点 #%1 [%2x%3]")
                    .arg(n->id())
                    .arg(qRound(r.width()))
                    .arg(qRound(r.height()));
    } else if (auto *tr = n->toTransform()) {
        const QTransform m = tr->matrix();
        label = QStringLiteral("变换节点 #%1 (dx:%2, dy:%3)")
                    .arg(n->id())
                    .arg(qRound(m.dx()))
                    .arg(qRound(m.dy()));
    } else {
        label = QStringLiteral("分组节点 #%1").arg(n->id());
    }
    const QString displayName = QStringLiteral("%1:%2").arg(order).arg(label);
    n->setName(displayName);
    m_displayNames.insert(n->id(), displayName);
    QVariantMap row;
    row.insert(QStringLiteral("id"), n->id());
    row.insert(QStringLiteral("name"), displayName);
    row.insert(QStringLiteral("type"), typeString(n->type()));
    row.insert(QStringLiteral("depth"), depth);
    row.insert(QStringLiteral("visible"), n->isVisible());
    row.insert(QStringLiteral("occluded"), worldHidden(n));
    row.insert(QStringLiteral("culled"), worldHidden(n));
    row.insert(QStringLiteral("childCount"), n->childCount());
    row.insert(QStringLiteral("parentId"), n->parent() ? n->parent()->id() : 0);
    tree->append(row);

    if (n->hasContent()) {
        auto *geo = n->toGeometry();
        const QRectF localRect = geo->boundingRect();
        const QTransform matrix = n->worldTransform();
        const QRect aabb = n->worldBounds();

        QVariantMap v;
        v.insert(QStringLiteral("id"), n->id());
        v.insert(QStringLiteral("name"), displayName);
        v.insert(QStringLiteral("type"), typeString(n->type()));
        v.insert(QStringLiteral("localX"), localRect.x());
        v.insert(QStringLiteral("localY"), localRect.y());
        v.insert(QStringLiteral("localWidth"), localRect.width());
        v.insert(QStringLiteral("localHeight"), localRect.height());
        v.insert(QStringLiteral("m11"), matrix.m11());
        v.insert(QStringLiteral("m12"), matrix.m12());
        v.insert(QStringLiteral("m21"), matrix.m21());
        v.insert(QStringLiteral("m22"), matrix.m22());
        v.insert(QStringLiteral("m13"), matrix.m13());
        v.insert(QStringLiteral("m23"), matrix.m23());
        v.insert(QStringLiteral("m33"), matrix.m33());
        v.insert(QStringLiteral("dx"), matrix.dx());
        v.insert(QStringLiteral("dy"), matrix.dy());
        v.insert(QStringLiteral("x"), aabb.x());
        v.insert(QStringLiteral("y"), aabb.y());
        v.insert(QStringLiteral("w"), aabb.width());
        v.insert(QStringLiteral("h"), aabb.height());
        v.insert(QStringLiteral("hasContent"), n->hasContent());
        v.insert(QStringLiteral("occluded"), worldHidden(n));
        v.insert(QStringLiteral("culled"), worldHidden(n));
        v.insert(QStringLiteral("visible"), n->isVisible());
        const QColor c = m_decor.value(n->id()).color;
        v.insert(QStringLiteral("color"), (c.isValid() ? c : QColor("#888888")).name());
        v.insert(QStringLiteral("isBackdrop"), n->needsBackdrop());
        v.insert(QStringLiteral("fullyOpaque"), geo->isFullyOpaque());
        v.insert(QStringLiteral("paintOrder"), order);
        visual->append(v);
    } else if (n->toClip()) {
        visual->append(clipVisualRow(n, displayName, order));
    }
    for (WSGDamageNode *c = n->firstChild(); c; c = c->nextSibling())
        collectVisual(c, visual, tree, depth + 1, paintOrder);
}

void DemoScene::refreshSelectedProps()
{
    QVariantMap p;
    WSGDamageNode *n = findNode(m_selectedId);
    if (!n) {
        m_selectedProps = p;
        emit selectedPropsChanged();
        return;
    }
    if (!m_displayNames.contains(m_selectedId))
        rebuildLists();
    p.insert(QStringLiteral("id"), n->id());
    p.insert(QStringLiteral("name"), m_displayNames.value(n->id(), n->name()));
    p.insert(QStringLiteral("type"), typeString(n->type()));
    p.insert(QStringLiteral("visible"), n->isVisible());
    p.insert(QStringLiteral("occluded"), worldHidden(n));
    p.insert(QStringLiteral("culled"), worldHidden(n));
    p.insert(QStringLiteral("isRoot"), n == m_root.get());
    p.insert(QStringLiteral("isGeometry"), n->toGeometry() != nullptr);
    p.insert(QStringLiteral("isTransform"), n->toTransform() != nullptr);
    p.insert(QStringLiteral("isClip"), n->toClip() != nullptr);
    p.insert(QStringLiteral("isBackdrop"), n->needsBackdrop());
    p.insert(QStringLiteral("canDelete"), n != m_root.get());
    p.insert(QStringLiteral("canRaise"), n->parent() && n->nextSibling());
    p.insert(QStringLiteral("canLower"), n->parent() && n->previousSibling());
    p.insert(QStringLiteral("canDirty"), n->toGeometry() != nullptr);
    if (auto *geo = n->toGeometry()) {
        const QRectF r = geo->boundingRect();
        p.insert(QStringLiteral("x"), r.x());
        p.insert(QStringLiteral("y"), r.y());
        p.insert(QStringLiteral("w"), r.width());
        p.insert(QStringLiteral("h"), r.height());
        p.insert(QStringLiteral("fullyOpaque"), geo->isFullyOpaque());
    }
    if (auto *clip = n->toClip()) {
        const QRectF r = clip->clipRect();
        p.insert(QStringLiteral("x"), r.x());
        p.insert(QStringLiteral("y"), r.y());
        p.insert(QStringLiteral("w"), r.width());
        p.insert(QStringLiteral("h"), r.height());
    }
    if (auto *tr = n->toTransform()) {
        const QTransform matrix = tr->matrix();
        p.insert(QStringLiteral("tx"), matrix.dx());
        p.insert(QStringLiteral("ty"), matrix.dy());
        p.insert(QStringLiteral("rotation"), m_rotation);
        p.insert(QStringLiteral("rotationAxis"), m_rotationAxis);
        p.insert(QStringLiteral("sx"), m_scaleX);
        p.insert(QStringLiteral("sy"), m_scaleY);
        p.insert(QStringLiteral("m11"), matrix.m11());
        p.insert(QStringLiteral("m12"), matrix.m12());
        p.insert(QStringLiteral("m21"), matrix.m21());
        p.insert(QStringLiteral("m22"), matrix.m22());
        p.insert(QStringLiteral("m13"), matrix.m13());
        p.insert(QStringLiteral("m23"), matrix.m23());
        p.insert(QStringLiteral("m33"), matrix.m33());
        p.insert(QStringLiteral("dx"), matrix.dx());
        p.insert(QStringLiteral("dy"), matrix.dy());
    }
    if (n->needsBackdrop()) {
        const Decor &decor = m_decor.value(n->id());
        p.insert(QStringLiteral("expansion"), decor.expansion);
        p.insert(QStringLiteral("clipExpansion"), decor.clipExpansion);
    }
    m_selectedProps = p;
    emit selectedPropsChanged();
}
