// Copyright (C) 2024 lbwtw <xiaoyaobing@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQuickItem>

class TQuickRadiusEffectPrivate;

class TQuickRadiusEffect : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(qreal topLeftRadius READ topLeftRadius WRITE setTopLeftRadius NOTIFY topLeftRadiusChanged RESET resetTopLeftRadius FINAL)
    Q_PROPERTY(qreal topRightRadius READ topRightRadius WRITE setTopRightRadius NOTIFY topRightRadiusChanged RESET resetTopRightRadius FINAL)
    Q_PROPERTY(qreal bottomLeftRadius READ bottomLeftRadius WRITE setBottomLeftRadius NOTIFY bottomLeftRadiusChanged RESET resetBottomLeftRadius FINAL)
    Q_PROPERTY(qreal bottomRightRadius READ bottomRightRadius WRITE setBottomRightRadius NOTIFY bottomRightRadiusChanged RESET resetBottomRightRadius FINAL)
    Q_PROPERTY(QQuickItem *sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)
    Q_PROPERTY(bool hideSource READ hideSource WRITE setHideSource NOTIFY hideSourceChanged)
    QML_NAMED_ELEMENT(TRadiusEffect)

public:
    explicit TQuickRadiusEffect(QQuickItem *parent = nullptr);
    ~TQuickRadiusEffect() override;

    qreal radius() const;
    void setRadius(qreal radius);

    qreal topLeftRadius() const;
    void setTopLeftRadius(qreal radius);
    void resetTopLeftRadius();
    qreal topRightRadius() const;
    void setTopRightRadius(qreal radius);
    void resetTopRightRadius();
    qreal bottomLeftRadius() const;
    void setBottomLeftRadius(qreal radius);
    void resetBottomLeftRadius();
    qreal bottomRightRadius() const;
    void setBottomRightRadius(qreal radius);
    void resetBottomRightRadius();

    QQuickItem *sourceItem() const;
    void setSourceItem(QQuickItem *item);

    bool hideSource() const;
    void setHideSource(bool hide);

Q_SIGNALS:
    void sourceItemChanged();
    void hideSourceChanged();
    void radiusChanged();
    void topLeftRadiusChanged();
    void topRightRadiusChanged();
    void bottomLeftRadiusChanged();
    void bottomRightRadiusChanged();

private Q_SLOTS:
    void sourceItemDestroyed(QObject *item);

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    TQuickRadiusEffect(TQuickRadiusEffectPrivate &dd, QQuickItem *parent = nullptr);

private:
    Q_DISABLE_COPY(TQuickRadiusEffect)
    Q_DECLARE_PRIVATE(TQuickRadiusEffect)
};
