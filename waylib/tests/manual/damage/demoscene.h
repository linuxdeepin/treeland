// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef DEMOSCENE_H
#define DEMOSCENE_H

#include "wsgdamagetracker.h"
#include "visualnodemodel.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVector>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE

class DemoScene : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(VisualNodeModel* visualNodes READ visualNodesModel CONSTANT)
    Q_PROPERTY(QVariantList treeNodes READ treeNodes NOTIFY treeNodesChanged)
    Q_PROPERTY(QVariantList damageRects READ damageRects NOTIFY damageChanged)
    Q_PROPERTY(QVariantList damageRectsB READ damageRectsB NOTIFY damageChanged)
    Q_PROPERTY(QVariantList damageFrames READ damageFrames NOTIFY damageChanged)
    Q_PROPERTY(QRect viewportA READ viewportA NOTIFY sceneChanged)
    Q_PROPERTY(QRect viewportB READ viewportB NOTIFY sceneChanged)
    Q_PROPERTY(quint64 selectedId READ selectedId WRITE setSelectedId NOTIFY selectedIdChanged)
    Q_PROPERTY(QVariantMap selectedProps READ selectedProps NOTIFY selectedPropsChanged)
    Q_PROPERTY(bool autoCommit READ autoCommit WRITE setAutoCommit NOTIFY autoCommitChanged)
    Q_PROPERTY(int refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)
    Q_PROPERTY(QVariantList demoScenes READ demoScenes CONSTANT)
    Q_PROPERTY(QString demoSceneName READ demoSceneName NOTIFY demoSceneChanged)
    Q_PROPERTY(bool demoRunning READ demoRunning WRITE setDemoRunning NOTIFY demoRunningChanged)

public:
    explicit DemoScene(QObject *parent = nullptr);
    ~DemoScene() override;

    VisualNodeModel *visualNodesModel() const { return m_visualNodeModel; }
    QVariantList treeNodes() const { return m_treeNodes; }
    QVariantList damageRects() const { return m_damageRects; }
    QVariantList damageRectsB() const { return m_damageRectsB; }
    QVariantList damageFrames() const { return m_damageFrames; }
    QRect viewportA() const { return m_viewportA; }
    QRect viewportB() const { return m_viewportB; }
    quint64 selectedId() const { return m_selectedId; }
    QVariantMap selectedProps() const { return m_selectedProps; }
    bool autoCommit() const { return m_autoCommit; }
    int refreshRate() const { return m_refreshRate; }
    QVariantList demoScenes() const;
    QString demoSceneName() const { return m_demoSceneName; }
    bool demoRunning() const { return m_demoRunning; }

    void setSelectedId(quint64 id);
    void setAutoCommit(bool enabled);
    void setRefreshRate(int refreshRate);
    void setDemoRunning(bool running);

    Q_INVOKABLE void loadDemoScene(const QString &name);
    Q_INVOKABLE void stepDemoFrame();
    Q_INVOKABLE void moveNode(quint64 nodeId, quint64 newParentId,
                              quint64 beforeSiblingId = 0);
    Q_INVOKABLE void activateNode(quint64 id);

    Q_INVOKABLE void addBasic();
    Q_INVOKABLE void addTransform();
    Q_INVOKABLE void addClip();
    Q_INVOKABLE void addGeometry();
    Q_INVOKABLE void addBackdrop();
    Q_INVOKABLE void addRenderer();
    Q_INVOKABLE void removeSelected();
    Q_INVOKABLE void raiseSelected();
    Q_INVOKABLE void lowerSelected();
    Q_INVOKABLE void setVisibleSelected(bool visible);
    Q_INVOKABLE void setRectSelected(qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE void setTranslationSelected(qreal x, qreal y);
    Q_INVOKABLE void setRotationSelected(qreal degrees, int axis = 2);
    Q_INVOKABLE void setScaleSelected(qreal sx, qreal sy);
    Q_INVOKABLE void setFullyOpaqueSelected(bool opaque);
    Q_INVOKABLE void setExpansionSelected(int px);
    Q_INVOKABLE void setClipExpansionSelected(bool clip);
    Q_INVOKABLE void markSelectedContentDirty();
    Q_INVOKABLE void markSelectedContentDirtyAt(qreal x, qreal y, qreal w, qreal h);
    Q_INVOKABLE void moveSelectedBy(qreal dx, qreal dy);
    Q_INVOKABLE void finishSelectedMove();
    Q_INVOKABLE void loadPreset(const QString &name);
    Q_INVOKABLE void commit();
    Q_INVOKABLE void clearTree();

signals:
    void sceneChanged();
    void treeNodesChanged();
    void damageChanged();
    void selectedIdChanged();
    void selectedPropsChanged();
    void autoCommitChanged();
    void refreshRateChanged();
    void demoSceneChanged();
    void demoRunningChanged();

private:
    struct Decor {
        QColor color;
        int expansion = 0;
        bool clipExpansion = true;
    };

    WSGDamageNode *findNode(quint64 id) const;
    WSGDamageNode *findNodeRecursive(WSGDamageNode *n, quint64 id) const;
    WSGDamageNode *parentForInsert() const;
    QColor nextColor();
    void rebuildLists();
    void rebuildVisualNodes();
    void collectVisualOnly(WSGDamageNode *n, QVector<QVariantMap> *visual, int *paintOrder);
    void collectVisual(WSGDamageNode *n, QVector<QVariantMap> *visual, QVariantList *tree,
                       int depth, int *paintOrder);
    void refreshSelectedProps();
    void maybeCommit();
    void updateDamage(bool rebuildScene);
    void resetRoot();
    void advanceDemoFrame();
    void buildDemoScene(const QString &name);
    bool isDescendantOf(const WSGDamageNode *node, const WSGDamageNode *ancestor) const;

    std::unique_ptr<WSGDamageNode> m_root;
    WSGDamageTracker m_tracker;
    QVector<WSGViewport> m_commitViewports;
    QHash<quint64, Decor> m_decor;
    QHash<quint64, QString> m_displayNames;
    VisualNodeModel *m_visualNodeModel = nullptr;
    QVariantList m_treeNodes;
    QVariantList m_damageRects;
    QVariantList m_damageRectsB;
    QVariantList m_damageFrames;
    QRect m_viewportA{0, 0, 360, 480};
    QRect m_viewportB{360, 0, 360, 480};
    quint64 m_selectedId = 0;
    QVariantMap m_selectedProps;
    bool m_autoCommit = true;
    bool m_dragFramePending = false;
    bool m_demoRunning = false;
    int m_refreshRate = 60;
    int m_demoFrame = 0;
    QString m_demoSceneName;
    quint64 m_demoNodeA = 0;
    quint64 m_demoNodeB = 0;
    quint64 m_demoNodeC = 0;
    int m_colorIndex = 0;
    int m_rotationAxis = 2;
    qreal m_rotation = 0;
    qreal m_scaleX = 1;
    qreal m_scaleY = 1;
    QTimer m_dragFrameTimer;
    QTimer m_demoTimer;
};

#endif
